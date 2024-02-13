#include "bytecode.hpp"

#include "ffi_adapter.hpp"
#include "interp.hpp"
#include "ir_impl.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/os/common.h"
#include "ntk/os/dl.h"
#include "ntk/os/syscall.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkBcOpcode s_ir2opcode[] = {
#define IR(NAME) NK_CAT(nkop_, NAME),
#include "nkb/ir.inl"
};

typedef NkSlice(NkBcInstr) NkBcInstrArray;

void inspect(NkBcInstrArray instrs, NkStream out) {
    auto inspect_ref = [&](NkBcRef const &ref, bool expand_values) {
        if (ref.kind == NkBcRef_None) {
            nk_stream_printf(out, "(null)");
            return;
        } else if (ref.kind == NkBcRef_Instr) {
            nk_stream_printf(out, "instr@%zi", ref.offset / sizeof(NkBcInstr));
            return;
        }
        for (usize i = 0; i < ref.indir; i++) {
            nk_stream_printf(out, "[");
        }
        switch (ref.kind) {
        case NkBcRef_Frame:
            nk_stream_printf(out, "frame");
            break;
        case NkBcRef_Arg:
            nk_stream_printf(out, "arg");
            break;
        case NkBcRef_Ret:
            nk_stream_printf(out, "ret");
            break;
        case NkBcRef_Data:
            if (expand_values) {
                nkirv_inspect(nkbc_deref(nullptr, ref), ref.type, out);
            } else {
                nk_stream_printf(out, "data");
            }
            break;
        default:
        case NkBcRef_None:
        case NkBcRef_Instr:
            nk_assert(!"unreachable");
            break;
        }
        if (ref.kind != NkBcRef_Data || !expand_values) {
            nk_stream_printf(out, "+%zx", ref.offset);
        }
        for (usize i = 0; i < ref.indir; i++) {
            nk_stream_printf(out, "]");
        }
        if (ref.post_offset) {
            nk_stream_printf(out, "+%zx", ref.post_offset);
        }
        if (ref.type) {
            nk_stream_printf(out, ":");
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
            nk_stream_printf(out, "(");
            for (usize i = 0; i < arg.refs.size; i++) {
                if (i) {
                    nk_stream_printf(out, ", ");
                }
                inspect_ref(arg.refs.data[i], expand_values);
            }
            nk_stream_printf(out, ")");
            break;

        default:
            break;
        }
    };

    for (auto const &instr : nk_iterate(instrs)) {
        nk_stream_printf(out, "%5zu", (&instr - instrs.data));
        nk_stream_printf(out, "%13s", nkbcOpcodeName(instr.code));

        for (usize i = 1; i < 3; i++) {
            if (instr.arg[i].kind != NkBcArg_None) {
                nk_stream_printf(out, ((i > 1) ? ", " : " "));
                inspect_arg(instr.arg[i], true);
            }
        }

        if (instr.arg[0].ref.kind != NkBcRef_None) {
            nk_stream_printf(out, " -> ");
            inspect_arg(instr.arg[0], false);
        }

        nk_stream_printf(out, "\n");
    }
}

NK_PRINTF_LIKE(2, 3) static void reportError(NkIrRunCtx ctx, char const *fmt, ...) {
    nk_assert(!ctx->error_str.data && "run error is already initialized");

    NkStringBuilder error{0, 0, 0, nk_arena_getAllocator(ctx->tmp_arena)};

    va_list ap;
    va_start(ap, fmt);
    nksb_vprintf(&error, fmt, ap);
    va_end(ap);

    ctx->error_str = {NK_SLICE_INIT(error)};
}

NkOsHandle getExternLib(NkIrRunCtx ctx, NkAtom name) {
    NK_PROF_FUNC();

    auto found = ctx->extern_libs.find(name);
    if (found) {
        return *found;
    } else {
        auto const name_str = nk_atom2cs(name);
        auto h_lib = nkdl_loadLibrary(name_str);
        if (nkos_handleIsZero(h_lib)) {
            auto const frame = nk_arena_grab(ctx->tmp_arena);
            defer {
                nk_arena_popFrame(ctx->tmp_arena, frame);
            };

            NkStringBuilder err_str{NKSB_INIT(nk_arena_getAllocator(ctx->tmp_arena))};
            nksb_printf(&err_str, "%s", nkdl_getLastErrorString());

            NkStringBuilder lib_name{NKSB_INIT(nk_arena_getAllocator(ctx->tmp_arena))};
            nksb_printf(&lib_name, "%s.%s", name_str, nkdl_file_extension);
            nksb_appendNull(&lib_name);
            h_lib = nkdl_loadLibrary(lib_name.data);

            if (nkos_handleIsZero(h_lib)) {
                nksb_clear(&lib_name);

                nksb_printf(&lib_name, "lib%s", name_str);
                nksb_appendNull(&lib_name);
                h_lib = nkdl_loadLibrary(lib_name.data);

                if (nkos_handleIsZero(h_lib)) {
                    nksb_clear(&lib_name);

                    nksb_printf(&lib_name, "lib%s.%s", name_str, nkdl_file_extension);
                    nksb_appendNull(&lib_name);
                    h_lib = nkdl_loadLibrary(lib_name.data);

                    if (nkos_handleIsZero(h_lib)) {
                        reportError(ctx, "failed to load extern library `%s`: " NKS_FMT, name_str, NKS_ARG(err_str));
                        return NK_OS_HANDLE_ZERO;
                    }
                }
            }
        }
        NK_LOG_DBG("loaded extern library `%s`: %zu", name_str, h_lib.val);
        return ctx->extern_libs.insert(name, h_lib);
    }
}

void *getExternSym(NkIrRunCtx ctx, NkAtom lib_hame, NkAtom name) {
    NK_PROF_FUNC();
    auto found = ctx->extern_syms.find(name);
    if (found) {
        return *found;
    } else {
        auto const name_str = nk_atom2cs(name);
        NkOsHandle h_lib = getExternLib(ctx, lib_hame);
        if (nkos_handleIsZero(h_lib)) {
            return NULL;
        }
        auto sym = nkdl_resolveSymbol(h_lib, name_str);
        if (!sym) {
            reportError(ctx, "failed to load extern symbol `%s`: %s", name_str, nkdl_getLastErrorString());
            return NULL;
        }
        NK_LOG_DBG("loaded extern symbol `%s`: %p", name_str, sym);
        return ctx->extern_syms.insert(name, sym);
    }
}

bool translateProc(NkIrRunCtx ctx, NkIrProc proc) {
    NK_PROF_FUNC();

    while (proc.idx >= ctx->procs.size) {
        nkda_append(&ctx->procs, nullptr);
    }

    if (ctx->procs.data[proc.idx]) {
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
        *(ctx->procs.data[proc.idx] = new (nk_allocT<NkBcProc_T>(ir.alloc)) NkBcProc_T{
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
        usize instr_index;
        usize arg_index;
        usize ref_index;
        usize target_id;
        ERelocType reloc_type;
        NkIrProcInfo const *proc_info{};
    };

    struct BlockInfo {
        usize first_instr;
    };

    NkDynArray(BlockInfo) block_info{0, 0, 0, tmp_alloc};
    NkDynArray(Reloc) relocs{0, 0, 0, tmp_alloc};
    NkDynArray(NkIrProc) referenced_procs{0, 0, 0, tmp_alloc};

    auto const get_data_addr = [&](usize index) {
        NK_PROF_SCOPE(nk_cs2s("get_data_addr"));
        while (index >= ctx->data.size) {
            nkda_append(&ctx->data, nullptr);
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
        return data;
    };

    auto const translate_ref =
        [&](usize instr_index, usize arg_index, usize ref_index, NkBcRef &ref, NkIrRef const &ir_ref) {
            NK_PROF_SCOPE(nk_cs2s("translate_ref"));
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
                ref.offset += (usize)get_data_addr(ir_ref.index);
                break;
            }
            case NkIrRef_Proc: {
                ref.kind = NkBcRef_Data;
                if (ir_ref.type->as.proc.info.call_conv == NkCallConv_Cdecl) {
                    nkda_append(
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
                    nkda_append(
                        &relocs,
                        (Reloc{
                            .instr_index = instr_index,
                            .arg_index = arg_index,
                            .ref_index = ref_index,
                            .target_id = ir_ref.index,
                            .reloc_type = Reloc_Proc,
                        }));
                }
                nkda_append(&referenced_procs, NkIrProc{ir_ref.index});
                break;
            }
            case NkIrRef_ExternData: {
                auto data = ir.extern_data.data[ir_ref.index];
                auto sym = getExternSym(ctx, data.lib, data.name);
                if (!sym) {
                    return false;
                }

                ref.kind = NkBcRef_Data;
                ref.offset += (usize)sym;
                break;
            }
            case NkIrRef_ExternProc: {
                auto proc = ir.extern_procs.data[ir_ref.index];
                auto sym = getExternSym(ctx, proc.lib, proc.name);
                if (!sym) {
                    return false;
                }

                auto sym_addr = nk_allocT<void *>(ir.alloc);
                *sym_addr = sym;

                ref.kind = NkBcRef_Data;
                ref.offset += (usize)sym_addr;
                break;
            }
            case NkIrRef_Address: {
                auto const &target_ref = ir.relocs.data[ir_ref.index];
                auto ref_addr = nk_allocT<void *>(ir.alloc);
                *ref_addr = get_data_addr(target_ref.index);

                ref.kind = NkBcRef_Data;
                ref.offset += (usize)ref_addr;
                break;
            }
            default:
                nk_assert(!"unreachable");
            case NkIrRef_None:
                break;
            }

            return true;
        };

    auto const translate_arg = [&](usize instr_index, usize arg_index, NkBcArg &arg, NkIrArg const &ir_arg) {
        NK_PROF_SCOPE(nk_cs2s("translate_arg"));
        switch (ir_arg.kind) {
        case NkIrArg_None: {
            arg.kind = NkBcArg_None;
            break;
        }

        case NkIrArg_Ref: {
            arg.kind = NkBcArg_Ref;
            if (!translate_ref(instr_index, arg_index, -1ul, arg.ref, ir_arg.ref)) {
                return false;
            }
            break;
        }

        case NkIrArg_RefArray: {
            arg.kind = NkBcArg_RefArray;
            auto refs = nk_allocT<NkBcRef>(ir.alloc, ir_arg.refs.size);
            for (usize i = 0; i < ir_arg.refs.size; i++) {
                if (!translate_ref(instr_index, arg_index, i, refs[i], ir_arg.refs.data[i])) {
                    return false;
                }
            }
            arg.refs = {refs, ir_arg.refs.size};
            break;
        }

        case NkIrArg_Label: {
            arg.kind = NkBcArg_Ref;
            arg.ref.kind = NkBcRef_Instr;
            nkda_append(
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
            nk_assert(!"unreachable");
        }

        return true;
    };

    for (auto block_id : nk_iterate(ir_proc.blocks)) {
        auto const &block = ir.blocks.data[block_id];

        while (block_id >= block_info.size) {
            nkda_append(&block_info, BlockInfo{});
        }
        block_info.data[block_id].first_instr = bc_proc.instrs.size;

        for (auto const &ir_instr_id : nk_iterate(block.instrs)) {
            auto const &ir_instr = ir.instrs.data[ir_instr_id];

            u16 code = s_ir2opcode[ir_instr.code];

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

                nk_assert(max_ref.type->kind == NkType_Numeric && min_ref.type->kind == NkType_Numeric);
                nk_assert(
                    NKIR_NUMERIC_TYPE_SIZE(max_ref.type->as.num.value_type) >
                    NKIR_NUMERIC_TYPE_SIZE(min_ref.type->as.num.value_type));

                if (NKIR_NUMERIC_IS_WHOLE(max_ref.type->as.num.value_type)) {
                    nk_assert(NKIR_NUMERIC_IS_WHOLE(min_ref.type->as.num.value_type));

                    if (ir_instr.code == nkir_ext) {
                        code = NKIR_NUMERIC_IS_SIGNED(max_ref.type->as.num.value_type) ? nkop_sext : nkop_zext;
                    }

                    switch (NKIR_NUMERIC_TYPE_SIZE(min_ref.type->as.num.value_type)) {
                    case 1:
                        code += 0 + nk_log2u32(NKIR_NUMERIC_TYPE_SIZE(max_ref.type->as.num.value_type));
                        break;
                    case 2:
                        code += 2 + nk_log2u32(NKIR_NUMERIC_TYPE_SIZE(max_ref.type->as.num.value_type));
                        break;
                    case 4:
                        code += 6;
                        break;
                    default:
                        nk_assert(!"unreachable");
                        break;
                    }
                } else {
                    nk_assert(max_ref.type->as.num.value_type == Float64);
                    nk_assert(min_ref.type->as.num.value_type == Float32);

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
                return false;
#endif // NK_SYSCALLS_AVAILABLE
                break;

#define SIZ_OP(NAME) case NK_CAT(nkir_, NAME):
#include "bytecode.inl"
                {
                    nk_assert(arg0.kind == NkIrArg_Ref || arg1.kind == NkIrArg_Ref);
                    auto const ref_type = arg0.kind == NkIrArg_Ref ? arg0.ref.type : arg1.ref.type;
                    if (ref_type->size <= 8 && nk_isZeroOrPowerOf2(ref_type->size)) {
                        code += 1 + nk_log2u64(ref_type->size);
                    }
                    break;
                }

#define BOOL_NUM_OP(NAME)
#define NUM_OP(NAME) case NK_CAT(nkir_, NAME):
#define INT_OP(NAME) case NK_CAT(nkir_, NAME):
#define FP2I_OP(NAME)
#include "bytecode.inl"
                nk_assert(arg0.ref.type->kind == NkType_Numeric);
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg0.ref.type->as.num.value_type);
                break;

#define BOOL_NUM_OP(NAME) case NK_CAT(nkir_, NAME):
#include "bytecode.inl"
                nk_assert(arg1.ref.type->kind == NkType_Numeric);
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                break;

            case nkir_comment:
                continue;
            }

            nkda_append(&bc_proc.instrs, NkBcInstr{});
            auto &instr = nk_slice_last(bc_proc.instrs);
            instr.code = code;
            for (usize ai = 0; ai < 3; ai++) {
                if (!translate_arg(bc_proc.instrs.size - 1, ai, instr.arg[ai], ir_instr.arg[ai])) {
                    return false;
                }
            }
        }
    }

    for (auto proc : nk_iterate(referenced_procs)) {
        if (!translateProc(ctx, proc)) {
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
            auto sym_addr = nk_allocT<void *>(ir.alloc);
            *sym_addr = ctx->procs.data[reloc.target_id];
            ref.offset = (usize)sym_addr;
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
            ref.offset = (usize)nk_native_makeClosure(&ctx->ffi_ctx, ctx->tmp_arena, ir.alloc, &call_data);
            break;
        }
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{};
    sb.alloc = tmp_alloc;
    inspect({NK_SLICE_INIT(bc_proc.instrs)}, nksb_getStream(&sb));
    NK_LOG_INF("proc %s\n" NKS_FMT "", nk_atom2cs(ir_proc.name), NKS_ARG(sb));
#endif // ENABLE_LOGGING

    return true;
}

} // namespace

char const *nkbcOpcodeName(u16 code) {
    switch (code) {
#define OP(NAME)              \
    case NK_CAT(nkop_, NAME): \
        return #NAME;
#define OPX(NAME, EXT)                                \
    case NK_CAT(nkop_, NK_CAT(NAME, NK_CAT(_, EXT))): \
        return #NAME "(" #EXT ")";
#include "bytecode.inl"

    default:
        return "";
    }
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir, NkArena *tmp_arena) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_allocT<NkIrRunCtx_T>(ir->alloc)) NkIrRunCtx_T{
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

    nk_freeT(ctx->ir->alloc, ctx);
}

bool nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, void **args, void **ret) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (!translateProc(ctx, proc)) {
        return false;
    }
    nkir_interp_invoke(ctx->procs.data[proc.idx], args, ret);
    return true;
}

NkString nkir_getRunErrorString(NkIrRunCtx ctx) {
    return ctx->error_str;
}
