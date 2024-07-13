#include "nkb/ir.h"

#include "cc_adapter.h"
#include "ir_impl.h"
#include "ntk/allocator.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/os/error.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
#include "translate2c.h"

namespace {

NK_LOG_USE_SCOPE(ir);

NkIrArg _arg(NkIrRef ref) {
    return {{.ref = ref}, NkIrArg_Ref};
}

NkIrArg _arg(NkIrProg ir, NkIrRefArray args) {
    auto refs = nk_allocT<NkIrRef>(ir->alloc, args.size);
    memcpy(refs, args.data, args.size * sizeof(NkIrRef));
    return {{.refs{refs, args.size}}, NkIrArg_RefArray};
}

NkIrArg _arg(NkIrLabel label) {
    return {{.id = label.idx}, NkIrArg_Label};
}

NkIrArg _arg(NkIrProg ir, NkString comment) {
    return {{.comment = nks_copy(ir->alloc, comment)}, NkIrArg_Comment};
}

} // namespace

char const *nkirOpcodeName(u8 code) {
    switch (code) {
#define IR(NAME)              \
    case NK_CAT(nkir_, NAME): \
        return #NAME;
#define DBL_IR(NAME1, NAME2)                             \
    case NK_CAT(nkir_, NK_CAT(NAME1, NK_CAT(_, NAME2))): \
        return #NAME1 " " #NAME2;
#include "nkb/ir.inl"

        default:
            return "";
    }
}

NkIrProg nkir_createProgram(NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    auto alloc = nk_arena_getAllocator(arena);
    return new (nk_allocT<NkIrProg_T>(alloc)) NkIrProg_T{
        .alloc = alloc,

        .procs{NKDA_INIT(alloc)},
        .blocks{NKDA_INIT(alloc)},
        .instrs{NKDA_INIT(alloc)},
        .data{NKDA_INIT(alloc)},
        .extern_data{NKDA_INIT(alloc)},
        .extern_procs{NKDA_INIT(alloc)},
        .relocs{NKDA_INIT(alloc)},
    };
}

NK_PRINTF_LIKE(2) static void reportError(NkIrProg ir, char const *fmt, ...) {
    nk_assert(!ir->error_str.data && "run error is already initialized");

    NkStringBuilder error{NKSB_INIT(ir->alloc)};

    va_list ap;
    va_start(ap, fmt);
    nksb_vprintf(&error, fmt, ap);
    va_end(ap);

    ir->error_str = {NKS_INIT(error)};
}

NkString nkir_getErrorString(NkIrProg ir) {
    return ir->error_str;
}

NkIrProc nkir_createProc(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    NkIrProc id{ir->procs.size};
    nkda_append(
        &ir->procs,
        {
            .blocks{NKDA_INIT(ir->alloc)},
            .locals{NKDA_INIT(ir->alloc)},
            .scopes{NKDA_INIT(ir->alloc)},

            .name{},
            .proc_t{},
            .arg_names{},
            .visibility{},

            .file{},
            .start_line{},
            .end_line{},

            .cur_block{},

            .frame_size{},
            .frame_align = 1,
            .cur_frame_size{},
        });
    return id;
}

NkIrLabel nkir_createLabel(NkIrProg ir, NkAtom name) {
    NK_LOG_TRC("%s", __func__);

    NkIrLabel id{ir->blocks.size};
    nkda_append(
        &ir->blocks,
        {
            .name = name,
            .instrs{NKDA_INIT(ir->alloc)},
        });
    return id;
}

void nkir_startProc(NkIrProg ir, NkIrProc _proc, NkIrProcDescr descr) {
    NK_LOG_TRC("%s", __func__);

    auto &proc = ir->procs.data[_proc.idx];

    proc.name = descr.name;
    proc.proc_t = descr.proc_t;
    nk_slice_copy(ir->alloc, &proc.arg_names, descr.arg_names);
    proc.file = descr.file;
    proc.start_line = descr.line;
    proc.visibility = descr.visibility;

    nkir_activateProc(ir, _proc);
}

void nkir_activateProc(NkIrProg ir, NkIrProc _proc) {
    NK_LOG_TRC("%s", __func__);

    ir->cur_proc = _proc;
}

void nkir_finishProc(NkIrProg ir, NkIrProc _proc, usize line) {
    NK_LOG_TRC("%s", __func__);

    auto &proc = ir->procs.data[_proc.idx];
    proc.end_line = line;
    proc.frame_size = nk_roundUpSafe(proc.frame_size, proc.frame_align);
}

void *nkir_getDataPtr(NkIrProg ir, NkIrData var) {
    NK_LOG_TRC("%s", __func__);

    auto &decl = ir->data.data[var.idx];
    if (!decl.data) {
        decl.data = nk_allocAligned(ir->alloc, decl.type->size, decl.type->align);
    }
    return decl.data;
}

void *nkir_dataRefDeref(NkIrProg ir, NkIrRef ref) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ref.kind == NkIrRef_Data && "data ref expected");
    auto data = (u8 *)nkir_getDataPtr(ir, {ref.index}) + ref.offset;
    int indir = ref.indir;
    while (indir--) {
        data = *(u8 **)data;
    }
    data += ref.post_offset;
    return data;
}

void nkir_emit(NkIrProg ir, NkIrInstr instr) {
    nkir_emitArray(ir, {&instr, 1});
}

void nkir_emitArray(NkIrProg ir, NkIrInstrArray instrs_array) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto &proc = ir->procs.data[ir->cur_proc.idx];

    for (auto const &instr : nk_iterate(instrs_array)) {
        if (instr.code == nkir_label) {
            proc.cur_block = instr.arg[1].id;
            nkda_append(&proc.blocks, proc.cur_block);
            continue;
        }

        nk_assert(proc.cur_block < ir->blocks.size && "no current block");
        auto &block = ir->blocks.data[proc.cur_block].instrs;

        nk_assert(
            instr.arg[0].kind != NkIrArg_Ref || instr.arg[0].ref.indir ||
            ((instr.arg[0].ref.kind != NkIrRef_Data || !ir->data.data[instr.arg[0].ref.index].read_only) &&
             instr.arg[0].ref.kind != NkIrRef_Arg));

        auto &instrs = ir->instrs;

        if (instr.code == nkir_ret && block.size && instrs.data[nk_slice_last(block)].code == nkir_ret) {
            continue;
        }

        usize id = instrs.size;
        nkda_append(&instrs, instr);
        nkda_append(&block, id);
    }
}

void nkir_emitArrayCopy(NkIrProg ir, NkIrInstrArray instrs, NkArena *tmp_arena) {
    NK_LOG_TRC("%s", __func__);

    auto frame = nk_arena_grab(tmp_arena);
    defer {
        nk_arena_popFrame(tmp_arena, frame);
    };

    struct LabelMapIt {
        usize old_label;
        usize new_label;
    };

    NkDynArray(LabelMapIt) label_map{NKDA_INIT(nk_arena_getAllocator(tmp_arena))};

    for (auto const &instr : nk_iterate(instrs)) {
        if (instr.code == nkir_label) {
            auto const old_label = instr.arg[1].id;
            auto const new_label = nkir_createLabel(ir, ir->blocks.data[old_label].name).idx;
            nkda_append(&label_map, {old_label, new_label});
        }
    }

    auto replace_label = [&label_map](NkIrArg &arg) {
        if (arg.kind == NkIrArg_Label) {
            for (auto const &it : nk_iterate(label_map)) {
                if (arg.id == it.old_label) {
                    arg.id = it.new_label;
                    return;
                }
            }
        }
    };

    for (auto const &instr : nk_iterate(instrs)) {
        auto new_instr = instr;
        replace_label(new_instr.arg[1]);
        replace_label(new_instr.arg[2]);
        nkir_emit(ir, new_instr);
    }
}

void nkir_setLine(NkIrProg ir, usize line) {
    NK_LOG_TRC("%s", __func__);

    ir->cur_line = line;
}

void nkir_enter(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto &proc = ir->procs.data[ir->cur_proc.idx];

    nkda_append(&proc.scopes, proc.cur_frame_size);
}

void nkir_leave(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto &proc = ir->procs.data[ir->cur_proc.idx];

    nk_assert(proc.scopes.size && "mismatched enter/leave");

    proc.cur_frame_size = nk_slice_last(proc.scopes);
    nkda_pop(&proc.scopes, 1);
}

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, NkAtom name, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto &proc = ir->procs.data[ir->cur_proc.idx];

    NkIrLocalVar id{proc.locals.size};
    proc.frame_align = nk_maxu(proc.frame_align, type->align);
    proc.cur_frame_size = nk_roundUpSafe(proc.cur_frame_size, type->align);
    nkda_append(&proc.locals, {name, type, proc.cur_frame_size});
    proc.cur_frame_size += type->size;
    proc.frame_size = nk_maxu(proc.frame_size, proc.cur_frame_size);
    return id;
}

NkIrData nkir_makeData(NkIrProg ir, NkAtom name, nktype_t type, NkIrVisibility vis) {
    NK_LOG_TRC("%s", __func__);

    NkIrData id{ir->data.size};
    nkda_append(&ir->data, {name, nullptr, type, vis, false});
    return id;
}

NkIrData nkir_makeRodata(NkIrProg ir, NkAtom name, nktype_t type, NkIrVisibility vis) {
    NK_LOG_TRC("%s", __func__);

    NkIrData id{ir->data.size};
    nkda_append(&ir->data, {name, nullptr, type, vis, true});
    return id;
}

NkIrExternData nkir_makeExternData(NkIrProg ir, NkAtom lib, NkAtom name, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternData id{ir->extern_data.size};
    nkda_append(&ir->extern_data, {lib, name, type});
    return id;
}

NkIrExternProc nkir_makeExternProc(NkIrProg ir, NkAtom lib, NkAtom name, nktype_t proc_t) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternProc id{ir->extern_procs.size};
    nkda_append(&ir->extern_procs, {lib, name, proc_t});
    return id;
}

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto const &proc = ir->procs.data[ir->cur_proc.idx];

    return {
        .index = var.idx,
        .offset = 0,
        .post_offset = 0,
        .type = proc.locals.data[var.idx].type,
        .kind = NkIrRef_Frame,
        .indir = 0,
    };
}

NkIrRef nkir_makeArgRef(NkIrProg ir, usize index) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto const &proc = ir->procs.data[ir->cur_proc.idx];

    auto const args_t = proc.proc_t->as.proc.info.args_t;
    nk_assert(index < args_t.size && "arg index out of range");
    return {
        .index = index,
        .offset = 0,
        .post_offset = 0,
        .type = args_t.data[index],
        .kind = NkIrRef_Arg,
        .indir = 0,
    };
}

NkIrRef nkir_makeRetRef(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto const &proc = ir->procs.data[ir->cur_proc.idx];

    return {
        .index = 0,
        .offset = 0,
        .post_offset = 0,
        .type = proc.proc_t->as.proc.info.ret_t,
        .kind = NkIrRef_Ret,
        .indir = 0,
    };
}

NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrData var) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = var.idx,
        .offset = 0,
        .post_offset = 0,
        .type = ir->data.data[var.idx].type,
        .kind = NkIrRef_Data,
        .indir = 0,
    };
}

NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.idx,
        .offset = 0,
        .post_offset = 0,
        .type = ir->procs.data[proc.idx].proc_t,
        .kind = NkIrRef_Proc,
        .indir = 0,
    };
}

NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData data) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = data.idx,
        .offset = 0,
        .post_offset = 0,
        .type = ir->extern_data.data[data.idx].type,
        .kind = NkIrRef_ExternData,
        .indir = 0,
    };
}

NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.idx,
        .offset = 0,
        .post_offset = 0,
        .type = ir->extern_procs.data[proc.idx].type,
        .kind = NkIrRef_ExternProc,
        .indir = 0,
    };
}

NkIrRef nkir_makeAddressRef(NkIrProg ir, NkIrRef ref, nktype_t ptr_t) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(ref.kind == NkIrRef_Data && "invalid address reference");

    auto const id = ir->relocs.size;
    nkda_append(&ir->relocs, ref);

    return {
        .index = id,
        .offset = 0,
        .post_offset = 0,
        .type = ptr_t,
        .kind = NkIrRef_Address,
        .indir = 0,
    };
}

NkIrRef nkir_makeVariadicMarkerRef(NkIrProg) {
    NK_LOG_TRC("%s", __func__);
    return {
        .index = 0,
        .offset = 0,
        .post_offset = 0,
        .type = NULL,
        .kind = NkIrRef_VariadicMarker,
        .indir = 0,
    };
}

NkIrInstr nkir_make_nop(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);
    return {{}, ir->cur_line, nkir_nop};
}

NkIrInstr nkir_make_ret(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);
    return {{}, ir->cur_line, nkir_ret};
}

NkIrInstr nkir_make_jmp(NkIrProg ir, NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(label)}, ir->cur_line, nkir_jmp};
}

NkIrInstr nkir_make_jmpz(NkIrProg ir, NkIrRef cond, NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(cond), _arg(label)}, ir->cur_line, nkir_jmpz};
}

NkIrInstr nkir_make_jmpnz(NkIrProg ir, NkIrRef cond, NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(cond), _arg(label)}, ir->cur_line, nkir_jmpnz};
}

NkIrInstr nkir_make_call(NkIrProg ir, NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(proc), _arg(ir, args)}, ir->cur_line, nkir_call};
}

#define UNA_IR(NAME)                                                            \
    NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef arg) { \
        NK_LOG_TRC("%s", __func__);                                             \
        return {{_arg(dst), _arg(arg), {}}, ir->cur_line, NK_CAT(nkir_, NAME)}; \
    }
#define BIN_IR(NAME)                                                                         \
    NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        NK_LOG_TRC("%s", __func__);                                                          \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, ir->cur_line, NK_CAT(nkir_, NAME)};       \
    }
#define DBL_IR(NAME1, NAME2)                                                                                      \
    NkIrInstr NK_CAT(nkir_make_, NK_CAT(NAME1, NK_CAT(_, NAME2)))(                                                \
        NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {                                                     \
        NK_LOG_TRC("%s", __func__);                                                                               \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, ir->cur_line, NK_CAT(nkir_, NK_CAT(NAME1, NK_CAT(_, NAME2)))}; \
    }
#include "nkb/ir.inl"

NkIrInstr nkir_make_syscall(NkIrProg ir, NkIrRef dst, NkIrRef n, NkIrRefArray args) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(n), _arg(ir, args)}, ir->cur_line, nkir_syscall};
}

NkIrInstr nkir_make_label(NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(label), {}}, 0, nkir_label};
}

NkIrInstr nkir_make_comment(NkIrProg ir, NkString comment) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(ir, comment), {}}, 0, nkir_comment};
}

bool nkir_write(NkIrProg ir, NkArena *arena, NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    NkPipeStream src{};
    bool res = nkcc_streamOpen(&src, conf);
    if (res) {
        nkir_translate2c(arena, ir, src.stream);
        if (nkcc_streamClose(&src)) {
            reportError(ir, "C compiler `" NKS_FMT "` returned nonzero exit code", NKS_ARG(conf.compiler_binary));
            return false;
        }
        return true;
    } else {
        reportError(
            ir, "failed to run C compiler `" NKS_FMT "`: %s", NKS_ARG(conf.compiler_binary), nk_getLastErrorString());
        return false;
    }
}

void nkir_inspectProgram(NkIrProg ir, NkStream out) {
    for (usize i = 0; i < ir->procs.size; i++) {
        nkir_inspectProc(ir, {i}, out);
    }

    nkir_inspectData(ir, out);
    nkir_inspectExternSyms(ir, out);
}

static bool isInlineDecl(NkIrDecl_T const &decl) {
    return decl.read_only && decl.visibility == NkIrVisibility_Local &&
           (decl.type->kind != NkIrType_Aggregate ||
            (decl.type->as.aggr.elems.size == 1 && decl.type->as.aggr.elems.data[0].type->size == 1));
}

void nkir_inspectData(NkIrProg ir, NkStream out) {
    if (ir->data.size) {
        bool printed = false;
        for (usize i = 0; i < ir->data.size; i++) {
            auto const &decl = ir->data.data[i];
            if (!isInlineDecl(decl)) {
                nk_stream_printf(out, "\n%s ", decl.read_only ? "const" : "data");
                if (decl.name != NK_ATOM_INVALID) {
                    auto const decl_name = nk_atom2s(decl.name);
                    nk_stream_printf(out, NKS_FMT, NKS_ARG(decl_name));
                } else {
                    nk_stream_printf(out, "%s%" PRIu64, decl.read_only ? "const" : "data", i);
                }
                nk_stream_printf(out, ": ");
                nkirt_inspect(decl.type, out);
                if (decl.data) {
                    nk_stream_printf(out, " = ");
                    nkirv_inspect(decl.data, decl.type, out);
                }
                printed = true;
            }
        }
        if (printed) {
            nk_stream_printf(out, "\n");
        }
    }
}

void inspectProcSignature(
    NkIrProcInfo const &proc_info,
    NkAtomArray arg_names,
    NkStream out,
    bool print_arg_names = true) {
    nk_stream_printf(out, "(");

    for (usize i = 0; i < proc_info.args_t.size; i++) {
        if (i) {
            nk_stream_printf(out, ", ");
        }
        if (print_arg_names) {
            if (i < arg_names.size && arg_names.data[i] != NK_ATOM_INVALID) {
                auto const name = nk_atom2s(arg_names.data[i]);
                nk_stream_printf(out, NKS_FMT ": ", NKS_ARG(name));
            } else {
                nk_stream_printf(out, "arg%" PRIu64 ": ", i);
            }
        }
        nkirt_inspect(proc_info.args_t.data[i], out);
    }

    if (proc_info.flags & NkProcVariadic) {
        if (proc_info.args_t.size) {
            nk_stream_printf(out, ", ");
        }
        nk_stream_printf(out, "...");
    }

    nk_stream_printf(out, ") ");

    nkirt_inspect(proc_info.ret_t, out);
}

void nkir_inspectExternSyms(NkIrProg ir, NkStream out) {
    if (ir->extern_data.size) {
        for (auto const &data : nk_iterate(ir->extern_data)) {
            nk_stream_printf(out, "\nextern");
            if (data.lib != NK_ATOM_INVALID) {
                nk_stream_printf(out, " \"%s\"", nk_atom2cs(data.lib));
            }
            nk_stream_printf(out, " data %s: ", nk_atom2cs(data.name));
            nkirt_inspect(data.type, out);
        }
        nk_stream_printf(out, "\n");
    }

    if (ir->extern_procs.size) {
        for (auto const &proc : nk_iterate(ir->extern_procs)) {
            nk_stream_printf(out, "\nextern");
            if (proc.lib != NK_ATOM_INVALID) {
                nk_stream_printf(out, " \"%s\"", nk_atom2cs(proc.lib));
            }
            nk_stream_printf(out, " proc %s", nk_atom2cs(proc.name));
            inspectProcSignature(proc.type->as.proc.info, {}, out, false);
        }
        nk_stream_printf(out, "\n");
    }
}

void nkir_inspectProc(NkIrProg ir, NkIrProc _proc, NkStream out) {
    auto const &proc = ir->procs.data[_proc.idx];

    nk_stream_printf(
        out,
        "\nproc%s %s",
        (proc.proc_t->as.proc.info.call_conv == NkCallConv_Cdecl ? " cdecl" : ""),
        nk_atom2cs(proc.name));
    inspectProcSignature(proc.proc_t->as.proc.info, proc.arg_names, out);

    nk_stream_printf(out, " {\n\n");

    if (proc.locals.size) {
        for (usize i = 0; i < proc.locals.size; i++) {
            if (proc.locals.data[i].name != NK_ATOM_INVALID) {
                auto const local_name = nk_atom2s(proc.locals.data[i].name);
                nk_stream_printf(out, NKS_FMT ": ", NKS_ARG(local_name));
            } else {
                nk_stream_printf(out, "var%" PRIu64 ": ", i);
            }
            nkirt_inspect(proc.locals.data[i].type, out);
            nk_stream_printf(out, "\n");
        }
        nk_stream_printf(out, "\n");
    }

    usize instr_index = 0;

    for (auto block_id : nk_iterate(proc.blocks)) {
        auto const &block = ir->blocks.data[block_id];

        nk_stream_printf(out, "%s#%zu\n", nk_atom2cs(block.name), block_id);

        for (auto instr_id : nk_iterate(block.instrs)) {
            auto const &instr = ir->instrs.data[instr_id];

            if (instr.code == nkir_comment) {
                nk_stream_printf(out, "%19s" NKS_FMT "\n", "// ", NKS_ARG(instr.arg[1].comment));
                continue;
            }

            nk_stream_printf(out, "%5zu |%8s", instr_index++, nkirOpcodeName(instr.code));

            for (usize i = 1; i < 3; i++) {
                auto const &arg = instr.arg[i];
                if (arg.kind != NkIrArg_None) {
                    nk_stream_printf(out, ((i > 1) ? ", " : " "));
                }
                switch (arg.kind) {
                    case NkIrArg_Ref: {
                        auto const &ref = arg.ref;
                        nkir_inspectRef(ir, _proc, ref, out);
                        break;
                    }
                    case NkIrArg_RefArray: {
                        nk_stream_printf(out, "(");
                        for (usize i = 0; i < arg.refs.size; i++) {
                            if (i) {
                                nk_stream_printf(out, ", ");
                            }
                            auto const &ref = arg.refs.data[i];
                            nkir_inspectRef(ir, _proc, ref, out);
                        }
                        nk_stream_printf(out, ")");
                        break;
                    }
                    case NkIrArg_Label:
                        if (arg.id < ir->blocks.size && ir->blocks.data[arg.id].name != NK_ATOM_INVALID) {
                            auto const name = nk_atom2s(ir->blocks.data[arg.id].name);
                            nk_stream_printf(out, NKS_FMT "#%zu", NKS_ARG(name), arg.id);
                        } else {
                            nk_stream_printf(out, "(null)");
                        }
                        break;
                    case NkIrArg_None:
                    default:
                        break;
                }
            }

            if (instr.arg[0].kind == NkIrArg_Ref && instr.arg[0].ref.kind != NkIrRef_None) {
                nk_stream_printf(out, " -> ");
                nkir_inspectRef(ir, _proc, instr.arg[0].ref, out);
            }

            nk_stream_printf(out, "\n");
        }

        nk_stream_printf(out, "\n");
    }

    nk_stream_printf(out, "}\n");
}

void nkir_inspectRef(NkIrProg ir, NkIrProc _proc, NkIrRef ref, NkStream out) {
    auto const &proc = ir->procs.data[_proc.idx];

    if (ref.kind == NkIrRef_None) {
        nk_stream_printf(out, "{}");
        return;
    } else if (ref.kind == NkIrRef_VariadicMarker) {
        nk_stream_printf(out, "...");
        return;
    }

    for (usize i = 0; i < ref.indir; i++) {
        nk_stream_printf(out, "[");
    }
    switch (ref.kind) {
        case NkIrRef_Frame: {
            auto const &decl = proc.locals.data[ref.index];
            if (decl.name != NK_ATOM_INVALID) {
                auto const local_name = nk_atom2s(decl.name);
                nk_stream_printf(out, NKS_FMT, NKS_ARG(local_name));
            } else {
                nk_stream_printf(out, "var%" PRIu64, ref.index);
            }
            break;
        }
        case NkIrRef_Arg: {
            if (ref.index < proc.arg_names.size && proc.arg_names.data[ref.index] != NK_ATOM_INVALID) {
                auto const name = nk_atom2s(proc.arg_names.data[ref.index]);
                nk_stream_printf(out, NKS_FMT, NKS_ARG(name));
            } else {
                nk_stream_printf(out, "arg%" PRIu64, ref.index);
            }
            break;
        }
        case NkIrRef_Ret:
            nk_stream_printf(out, "ret");
            break;
        case NkIrRef_Data: {
            auto const &decl = ir->data.data[ref.index];
            if (isInlineDecl(decl)) {
                void *data = nkir_dataRefDeref(ir, ref);
                nkirv_inspect(data, ref.type, out);
            } else {
                if (decl.name != NK_ATOM_INVALID) {
                    auto const decl_name = nk_atom2s(decl.name);
                    nk_stream_printf(out, NKS_FMT, NKS_ARG(decl_name));
                } else {
                    nk_stream_printf(out, "%s%" PRIu64, decl.read_only ? "const" : "data", ref.index);
                }
            }
            break;
        }
        case NkIrRef_Proc: {
            auto const name = nk_atom2s(ir->procs.data[ref.index].name);
            nk_stream_printf(out, NKS_FMT, NKS_ARG(name));
            break;
        }
        case NkIrRef_ExternData: {
            auto const name = nk_atom2s(ir->extern_data.data[ref.index].name);
            nk_stream_printf(out, NKS_FMT, NKS_ARG(name));
            break;
        }
        case NkIrRef_ExternProc: {
            auto const name = nk_atom2s(ir->extern_procs.data[ref.index].name);
            nk_stream_printf(out, NKS_FMT, NKS_ARG(name));
            break;
        }
        case NkIrRef_Address: {
            nk_stream_printf(out, "&");
            auto const &reloc_ref = ir->relocs.data[ref.index];
            nkir_inspectRef(ir, _proc, reloc_ref, out);
            break;
        }
        case NkIrRef_None:
        case NkIrRef_VariadicMarker:
        default:
            nk_assert(!"unreachable");
            break;
    }
    if (ref.offset) {
        nk_stream_printf(out, "+%" PRIu64, ref.offset);
    }
    for (usize i = 0; i < ref.indir; i++) {
        nk_stream_printf(out, "]");
    }
    if (ref.post_offset) {
        nk_stream_printf(out, "+%" PRIu64, ref.post_offset);
    }
    if (ref.kind != NkIrRef_Address) {
        nk_stream_printf(out, ":");
        nkirt_inspect(ref.type, out);
    }
}
