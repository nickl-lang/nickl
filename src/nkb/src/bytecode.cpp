#include "bytecode.hpp"

#include <cassert>

#include "ffi_adapter.hpp"
#include "interp.hpp"
#include "ir_impl.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/id.h"
#include "ntk/logger.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/sys/dl.h"
#include "ntk/sys/syscall.h"

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkBcOpcode s_ir2opcode[] = {
#define IR(NAME) CAT(nkop_, NAME),
#include "nkb/ir.inl"
};

#ifdef ENABLE_LOGGING
nkav_typedef(NkBcInstr, NkBcInstrView);

void inspect(NkBcInstrView instrs, nk_stream out) {
    auto inspect_ref = [&](NkBcRef const &ref, bool expand_values) {
        if (ref.kind == NkBcRef_None) {
            nk_printf(out, "(null)");
            return;
        } else if (ref.kind == NkBcRef_Instr) {
            nk_printf(out, "instr@%zi", ref.offset / sizeof(NkBcInstr));
            return;
        }
        for (size_t i = 0; i < ref.indir; i++) {
            nk_printf(out, "[");
        }
        switch (ref.kind) {
        case NkBcRef_Frame:
            nk_printf(out, "frame");
            break;
        case NkBcRef_Arg:
            nk_printf(out, "arg");
            break;
        case NkBcRef_Ret:
            nk_printf(out, "ret");
            break;
        case NkBcRef_Data:
            if (expand_values) {
                nkirv_inspect(nkbc_deref(nullptr, ref), ref.type, out);
            } else {
                nk_printf(out, "data");
            }
            break;
        default:
        case NkBcRef_None:
        case NkBcRef_Instr:
            assert(!"unreachable");
            break;
        }
        if (ref.kind != NkBcRef_Data || !expand_values) {
            nk_printf(out, "+%zx", ref.offset);
        }
        for (size_t i = 0; i < ref.indir; i++) {
            nk_printf(out, "]");
        }
        if (ref.post_offset) {
            nk_printf(out, "+%zx", ref.post_offset);
        }
        if (ref.type) {
            nk_printf(out, ":");
            nkirt_inspect(ref.type, out);
        }
    };

    auto inspect_arg = [&](NkBcArg const &arg, bool expand_values) {
        switch (arg.kind) {
        case NkBcArg_Ref: {
            inspect_ref(arg.ref, expand_values);
            break;
        }

        case NkBcArg_RefArray:
            nk_printf(out, "(");
            for (size_t i = 0; i < arg.refs.size; i++) {
                if (i) {
                    nk_printf(out, ", ");
                }
                inspect_ref(arg.refs.data[i], expand_values);
            }
            nk_printf(out, ")");
            break;

        default:
            break;
        }
    };

    for (auto const &instr : nk_iterate(instrs)) {
        nk_printf(out, "%5zu", (&instr - instrs.data));
        nk_printf(out, "%13s", nkbcOpcodeName(instr.code));

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].kind != NkBcArg_None) {
                nk_printf(out, ((i > 1) ? ", " : " "));
                inspect_arg(instr.arg[i], true);
            }
        }

        if (instr.arg[0].ref.kind != NkBcRef_None) {
            nk_printf(out, " -> ");
            inspect_arg(instr.arg[0], false);
        }

        nk_printf(out, "\n");
    }
}
#endif // ENABLE_LOGGING

NK_PRINTF_LIKE(2, 3) static void reportError(NkIrRunCtx ctx, char const *fmt, ...) {
    assert(!ctx->error_str.data && "run error is already initialized");

    NkStringBuilder error{0, 0, 0, nk_arena_getAllocator(ctx->tmp_arena)};

    va_list ap;
    va_start(ap, fmt);
    nksb_vprintf(&error, fmt, ap);
    va_end(ap);

    ctx->error_str = {nkav_init(error)};
}

nkdl_t getExternLib(NkIrRunCtx ctx, nkid name) {
    ProfBeginFunc();

    auto found = ctx->extern_libs.find(name);
    if (found) {
        ProfEndBlock();
        return *found;
    } else {
        auto const name_str = nkid2cs(name);
        auto lib = nk_load_library(name_str);
        if (!lib) {
            auto const frame = nk_arena_grab(ctx->tmp_arena);
            defer {
                nk_arena_popFrame(ctx->tmp_arena, frame);
            };

            NkStringBuilder err_str{0, 0, 0, nk_arena_getAllocator(ctx->tmp_arena)};
            nksb_printf(&err_str, "%s", nkdl_getLastErrorString());

            NkStringBuilder lib_name{0, 0, 0, nk_arena_getAllocator(ctx->tmp_arena)};
            nksb_printf(&lib_name, "%s.%s", name_str, nk_dl_extension);
            nksb_append_null(&lib_name);
            lib = nk_load_library(lib_name.data);

            if (!lib) {
                nksb_clear(&lib_name);

                nksb_printf(&lib_name, "lib%s", name_str);
                nksb_append_null(&lib_name);
                lib = nk_load_library(lib_name.data);

                if (!lib) {
                    nksb_clear(&lib_name);

                    nksb_printf(&lib_name, "lib%s.%s", name_str, nk_dl_extension);
                    nksb_append_null(&lib_name);
                    lib = nk_load_library(lib_name.data);

                    if (!lib) {
                        reportError(ctx, "failed to load extern library `%s`: " nks_Fmt, name_str, nks_Arg(err_str));
                        ProfEndBlock();
                        return NULL;
                    }
                }
            }
        }
        NK_LOG_DBG("loaded extern library `%s`: %p", name_str, lib);
        ProfEndBlock();
        return ctx->extern_libs.insert(name, lib);
    }
}

void *getExternSym(NkIrRunCtx ctx, nkid lib_hame, nkid name) {
    ProfBeginFunc();
    auto found = ctx->extern_syms.find(name);
    if (found) {
        ProfEndBlock();
        return *found;
    } else {
        auto const name_str = nkid2cs(name);
        nkdl_t lib = getExternLib(ctx, lib_hame);
        if (!lib) {
            ProfEndBlock();
            return NULL;
        }
        auto sym = nk_resolve_symbol(lib, name_str);
        if (!sym) {
            reportError(ctx, "failed to load extern symbol `%s`: %s", name_str, nkdl_getLastErrorString());
            ProfEndBlock();
            return NULL;
        }
        NK_LOG_DBG("loaded extern symbol `%s`: %p", name_str, sym);
        ProfEndBlock();
        return ctx->extern_libs.insert(name, sym);
    }
}

bool translateProc(NkIrRunCtx ctx, NkIrProc proc) {
    ProfBeginFunc();

    while (proc.idx >= ctx->procs.size) {
        nkar_append(&ctx->procs, nullptr);
    }

    if (ctx->procs.data[proc.idx]) {
        ProfEndBlock();
        return true;
    }

    NK_LOG_TRC("%s", __func__);

    auto const tmp_alloc = nk_arena_getAllocator(ctx->tmp_arena);
    auto const frame = nk_arena_grab(ctx->tmp_arena);
    defer {
        nk_arena_popFrame(ctx->tmp_arena, frame);
    };

    auto const &ir = *ctx->ir;
    auto const &ir_proc = ir.procs.data[proc.idx];

    auto &bc_proc =
        *(ctx->procs.data[proc.idx] = new (nk_alloc_t<NkBcProc_T>(ir.alloc)) NkBcProc_T{
              .ctx = ctx,
              .frame_size = ir_proc.frame_size,
              .frame_align = ir_proc.frame_align,
              .instrs{0, 0, 0, ir.alloc},
          });

    enum ERelocType {
        Reloc_Block,
        Reloc_Proc,
        Reloc_Closure,
    };

    struct Reloc {
        size_t instr_index;
        size_t arg_index;
        size_t ref_index;
        size_t target_id;
        ERelocType reloc_type;
        NkIrProcInfo const *proc_info{};
    };

    struct BlockInfo {
        size_t first_instr;
    };

    nkar_type(BlockInfo) block_info{0, 0, 0, tmp_alloc};
    nkar_type(Reloc) relocs{0, 0, 0, tmp_alloc};
    nkar_type(NkIrProc) referenced_procs{0, 0, 0, tmp_alloc};

    auto const get_data_addr = [&](size_t index) {
        ProfBeginBlock("get_data_addr", sizeof("get_data_addr") - 1);
        while (index >= ctx->data.size) {
            nkar_append(&ctx->data, nullptr);
        }
        auto &data = ctx->data.data[index];
        if (!data) {
            auto const &decl = ir.data.data[index];
            if (decl.data && decl.read_only) {
                data = decl.data;
            } else {
                data = nk_allocAligned(ir.alloc, decl.type->size, decl.type->align);
                if (decl.data) {
                    memcpy(data, decl.data, decl.type->size);
                } else {
                    memset(data, 0, decl.type->size);
                }
            }
        }
        ProfEndBlock();
        return data;
    };

    auto const translate_ref =
        [&](size_t instr_index, size_t arg_index, size_t ref_index, NkBcRef &ref, NkIrRef const &ir_ref) {
            ProfBeginBlock("translate_ref", sizeof("translate_ref") - 1);
            ref = {
                .offset = ir_ref.offset,
                .post_offset = ir_ref.post_offset,
                .type = ir_ref.type,
                .kind = (NkBcRefKind)ir_ref.kind,
                .indir = ir_ref.indir,
            };

            switch (ir_ref.kind) {
            case NkIrRef_Frame:
                ref.offset += ir_proc.locals.data[ir_ref.index].offset;
                break;
            case NkIrRef_Arg:
                ref.offset += ir_ref.index * sizeof(void *);
                ref.indir++;
                break;
            case NkIrRef_Ret:
                ref.indir++;
                break;
            case NkIrRef_Data: {
                ref.offset += (size_t)get_data_addr(ir_ref.index);
                break;
            }
            case NkIrRef_Proc: {
                ref.kind = NkBcRef_Data;
                if (ir_ref.type->as.proc.info.call_conv == NkCallConv_Cdecl) {
                    nkar_append(
                        &relocs,
                        (Reloc{
                            .instr_index = instr_index,
                            .arg_index = arg_index,
                            .ref_index = ref_index,
                            .target_id = ir_ref.index,
                            .reloc_type = Reloc_Closure,
                            .proc_info = &ir_ref.type->as.proc.info,
                        }));
                } else {
                    nkar_append(
                        &relocs,
                        (Reloc{
                            .instr_index = instr_index,
                            .arg_index = arg_index,
                            .ref_index = ref_index,
                            .target_id = ir_ref.index,
                            .reloc_type = Reloc_Proc,
                        }));
                }
                nkar_append(&referenced_procs, NkIrProc{ir_ref.index});
                break;
            }
            case NkIrRef_ExternData: {
                auto data = ir.extern_data.data[ir_ref.index];
                auto sym = getExternSym(ctx, data.lib, data.name);
                if (!sym) {
                    ProfEndBlock();
                    return false;
                }

                ref.kind = NkBcRef_Data;
                ref.offset += (size_t)sym;
                break;
            }
            case NkIrRef_ExternProc: {
                auto proc = ir.extern_procs.data[ir_ref.index];
                auto sym = getExternSym(ctx, proc.lib, proc.name);
                if (!sym) {
                    ProfEndBlock();
                    return false;
                }

                auto sym_addr = nk_alloc_t<void *>(ir.alloc);
                *sym_addr = sym;

                ref.kind = NkBcRef_Data;
                ref.offset += (size_t)sym_addr;
                break;
            }
            case NkIrRef_Address: {
                auto const &target_ref = ir.relocs.data[ir_ref.index];
                auto ref_addr = nk_alloc_t<void *>(ir.alloc);
                *ref_addr = get_data_addr(target_ref.index);

                ref.kind = NkBcRef_Data;
                ref.offset += (size_t)ref_addr;
                break;
            }
            default:
                assert(!"unreachable");
            case NkIrRef_None:
                break;
            }

            ProfEndBlock();
            return true;
        };

    auto const translate_arg = [&](size_t instr_index, size_t arg_index, NkBcArg &arg, NkIrArg const &ir_arg) {
        ProfBeginBlock("translate_arg", sizeof("translate_arg") - 1);
        switch (ir_arg.kind) {
        case NkIrArg_None: {
            arg.kind = NkBcArg_None;
            break;
        }

        case NkIrArg_Ref: {
            arg.kind = NkBcArg_Ref;
            if (!translate_ref(instr_index, arg_index, -1ul, arg.ref, ir_arg.ref)) {
                ProfEndBlock();
                return false;
            }
            break;
        }

        case NkIrArg_RefArray: {
            arg.kind = NkBcArg_RefArray;
            auto refs = nk_alloc_t<NkBcRef>(ir.alloc, ir_arg.refs.size);
            for (size_t i = 0; i < ir_arg.refs.size; i++) {
                if (!translate_ref(instr_index, arg_index, i, refs[i], ir_arg.refs.data[i])) {
                    ProfEndBlock();
                    return false;
                }
            }
            arg.refs = {refs, ir_arg.refs.size};
            break;
        }

        case NkIrArg_Label: {
            arg.kind = NkBcArg_Ref;
            arg.ref.kind = NkBcRef_Instr;
            nkar_append(
                &relocs,
                (Reloc{
                    .instr_index = instr_index,
                    .arg_index = arg_index,
                    .ref_index = -1ul,
                    .target_id = ir_arg.id,
                    .reloc_type = Reloc_Block,
                }));
            break;
        }

        case NkIrArg_Comment:
        default:
            assert(!"unreachable");
        }

        ProfEndBlock();
        return true;
    };

    for (auto block_id : nk_iterate(ir_proc.blocks)) {
        auto const &block = ir.blocks.data[block_id];

        while (block_id >= block_info.size) {
            nkar_append(&block_info, BlockInfo{});
        }
        block_info.data[block_id].first_instr = bc_proc.instrs.size;

        for (auto const &ir_instr_id : nk_iterate(block.instrs)) {
            auto const &ir_instr = ir.instrs.data[ir_instr_id];

            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg0 = ir_instr.arg[0];
            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call: {
                if (arg1.ref.kind == NkIrRef_ExternProc ||
                    (arg1.ref.kind == NkIrRef_Proc && arg1.ref.type->as.proc.info.call_conv != NkCallConv_Nk)) {
                    code = nkop_call_ext;
                } else if (arg1.ref.kind == NkIrRef_Proc) {
                    code = nkop_call_jmp;
                }
                break;
            }

            case nkir_ext:
            case nkir_trunc: {
                auto const &max_ref = ir_instr.arg[ir_instr.code == nkir_trunc].ref;
                auto const &min_ref = ir_instr.arg[ir_instr.code == nkir_ext].ref;

                assert(max_ref.type->kind == NkType_Numeric && min_ref.type->kind == NkType_Numeric);
                assert(
                    NKIR_NUMERIC_TYPE_SIZE(max_ref.type->as.num.value_type) >
                    NKIR_NUMERIC_TYPE_SIZE(min_ref.type->as.num.value_type));

                if (NKIR_NUMERIC_IS_WHOLE(max_ref.type->as.num.value_type)) {
                    assert(NKIR_NUMERIC_IS_WHOLE(min_ref.type->as.num.value_type));

                    if (ir_instr.code == nkir_ext) {
                        code = NKIR_NUMERIC_IS_SIGNED(max_ref.type->as.num.value_type) ? nkop_sext : nkop_zext;
                    }

                    switch (NKIR_NUMERIC_TYPE_SIZE(min_ref.type->as.num.value_type)) {
                    case 1:
                        code += 0 + log2u32(NKIR_NUMERIC_TYPE_SIZE(max_ref.type->as.num.value_type));
                        break;
                    case 2:
                        code += 2 + log2u32(NKIR_NUMERIC_TYPE_SIZE(max_ref.type->as.num.value_type));
                        break;
                    case 4:
                        code += 6;
                        break;
                    default:
                        assert(!"unreachable");
                        break;
                    }
                } else {
                    assert(max_ref.type->as.num.value_type == Float64);
                    assert(min_ref.type->as.num.value_type == Float32);

                    code = ir_instr.code == nkir_ext ? nkop_fext : nkop_ftrunc;
                }
                break;
            }

            case nkir_fp2i: {
                code = arg1.ref.type->as.num.value_type == Float32 ? nkop_fp2i_32 : nkop_fp2i_64;
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg0.ref.type->as.num.value_type);
                break;
            }
            case nkir_i2fp: {
                code = arg0.ref.type->as.num.value_type == Float32 ? nkop_i2fp_32 : nkop_i2fp_64;
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                break;
            }

            case nkir_syscall:
#if NK_SYSCALLS_AVAILABLE
                code += 1 + ir_instr.arg[2].refs.size;
#else  // NK_SYSCALLS_AVAILABLE
                reportError(ctx, "syscalls are not available on the host platform");
                ProfEndBlock();
                return false;
#endif // NK_SYSCALLS_AVAILABLE
                break;

#define SIZ_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                {
                    assert(arg0.kind == NkIrArg_Ref || arg1.kind == NkIrArg_Ref);
                    auto const ref_type = arg0.kind == NkIrArg_Ref ? arg0.ref.type : arg1.ref.type;
                    if (ref_type->size <= 8 && isZeroOrPowerOf2(ref_type->size)) {
                        code += 1 + log2u64(ref_type->size);
                    }
                    break;
                }

#define BOOL_NUM_OP(NAME)
#define NUM_OP(NAME) case CAT(nkir_, NAME):
#define INT_OP(NAME) case CAT(nkir_, NAME):
#define FP2I_OP(NAME)
#include "bytecode.inl"
                assert(arg0.ref.type->kind == NkType_Numeric);
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg0.ref.type->as.num.value_type);
                break;

#define BOOL_NUM_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                assert(arg1.ref.type->kind == NkType_Numeric);
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                break;

            case nkir_comment:
                continue;
            }

            nkar_append(&bc_proc.instrs, NkBcInstr{});
            auto &instr = nkav_last(bc_proc.instrs);
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                if (!translate_arg(bc_proc.instrs.size - 1, ai, instr.arg[ai], ir_instr.arg[ai])) {
                    ProfEndBlock();
                    return false;
                }
            }
        }
    }

    for (auto proc : nk_iterate(referenced_procs)) {
        if (!translateProc(ctx, proc)) {
            ProfEndBlock();
            return false;
        }
    }

    for (auto const &reloc : nk_iterate(relocs)) {
        auto &arg = bc_proc.instrs.data[reloc.instr_index].arg[reloc.arg_index];
        auto &ref = reloc.ref_index == -1ul ? arg.ref : arg.refs.data[reloc.ref_index];

        switch (reloc.reloc_type) {
        case Reloc_Block: {
            ref.offset = block_info.data[reloc.target_id].first_instr * sizeof(NkBcInstr);
            break;
        }
        case Reloc_Proc: {
            auto sym_addr = nk_alloc_t<void *>(ir.alloc);
            *sym_addr = ctx->procs.data[reloc.target_id];
            ref.offset = (size_t)sym_addr;
            break;
        }
        case Reloc_Closure: {
            NkNativeCallData const call_data{
                .proc{.bytecode = ctx->procs.data[reloc.target_id]},
                .nfixedargs = reloc.proc_info->args_t.size,
                .is_variadic = (bool)(reloc.proc_info->flags & NkProcVariadic),
                .argv{},
                .argt = reloc.proc_info->args_t.data,
                .argc = reloc.proc_info->args_t.size,
                .retv{},
                .rett = reloc.proc_info->ret_t,
            };
            ref.offset = (size_t)nk_native_makeClosure(&ctx->ffi_ctx, ctx->tmp_arena, ir.alloc, &call_data);
            break;
        }
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{};
    sb.alloc = tmp_alloc;
    inspect({nkav_init(bc_proc.instrs)}, nksb_getStream(&sb));
    NK_LOG_INF("proc %s\n" nks_Fmt "", nkid2cs(ir_proc.name), nks_Arg(sb));
#endif // ENABLE_LOGGING

    ProfEndBlock();
    return true;
}

} // namespace

char const *nkbcOpcodeName(uint16_t code) {
    switch (code) {
#define OP(NAME)           \
    case CAT(nkop_, NAME): \
        return #NAME;
#define OPX(NAME, EXT)                       \
    case CAT(nkop_, CAT(NAME, CAT(_, EXT))): \
        return #NAME "(" #EXT ")";
#include "bytecode.inl"

    default:
        return "";
    }
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir, NkArena *tmp_arena) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc_t<NkIrRunCtx_T>(ir->alloc)) NkIrRunCtx_T{
        .ir = ir,
        .tmp_arena = tmp_arena,

        .procs{0, 0, 0, ir->alloc},
        .data{0, 0, 0, ir->alloc},
        .extern_libs = decltype(NkIrRunCtx_T::extern_libs)::create(ir->alloc),
        .extern_syms = decltype(NkIrRunCtx_T::extern_syms)::create(ir->alloc),

        .ffi_ctx{
            .alloc = ir->alloc,
        },
    };
}

void nkir_freeRunCtx(NkIrRunCtx ctx) {
    NK_LOG_TRC("%s", __func__);

    ctx->ffi_ctx.typemap.deinit();

    nk_free_t(ctx->ir->alloc, ctx);
}

bool nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, void **args, void **ret) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    if (!translateProc(ctx, proc)) {
        ProfEndBlock();
        return false;
    }
    nkir_interp_invoke(ctx->procs.data[proc.idx], args, ret);
    ProfEndBlock();
    return true;
}

nks nkir_getRunErrorString(NkIrRunCtx ctx) {
    return ctx->error_str;
}
