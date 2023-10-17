#include "nkb/ir.h"

#include <algorithm>
#include <cassert>
#include <new>

#include "cc_adapter.h"
#include "ir_impl.hpp"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/id.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/sys/error.h"
#include "translate2c.h"

namespace {

NK_LOG_USE_SCOPE(ir);

NkIrArg _arg(NkIrRef ref) {
    return {{.ref = ref}, NkIrArg_Ref};
}

NkIrArg _arg(NkIrProg ir, NkIrRefArray args) {
    auto refs = nk_alloc_t<NkIrRef>(ir->alloc, args.size);
    memcpy(refs, args.data, args.size * sizeof(NkIrRef));
    return {{.refs{refs, args.size}}, NkIrArg_RefArray};
}

NkIrArg _arg(NkIrLabel label) {
    return {{.id = label.idx}, NkIrArg_Label};
}

NkIrArg _arg(NkIrProg ir, nks comment) {
    return {{.comment = nk_strcpy(ir->alloc, comment)}, NkIrArg_Comment};
}

} // namespace

char const *nkirOpcodeName(uint8_t code) {
    switch (code) {
#define IR(NAME)           \
    case CAT(nkir_, NAME): \
        return #NAME;
#define DBL_IR(NAME1, NAME2)                    \
    case CAT(nkir_, CAT(NAME1, CAT(_, NAME2))): \
        return #NAME1 " " #NAME2;
#include "nkb/ir.inl"

    default:
        return "";
    }
}

NkIrProg nkir_createProgram(NkAllocator alloc) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc_t<NkIrProg_T>(alloc)) NkIrProg_T{
        .alloc = alloc,

        .procs{0, 0, 0, alloc},
        .blocks{0, 0, 0, alloc},
        .instrs{0, 0, 0, alloc},
        .globals{0, 0, 0, alloc},
        .consts{0, 0, 0, alloc},
        .extern_data{0, 0, 0, alloc},
        .extern_procs{0, 0, 0, alloc},
        .relocs{0, 0, 0, alloc},
    };
}

void nkir_freeProgram(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    nkar_free(&ir->procs);
    nkar_free(&ir->blocks);
    nkar_free(&ir->instrs);
    nkar_free(&ir->globals);
    nkar_free(&ir->consts);
    nkar_free(&ir->extern_data);
    nkar_free(&ir->extern_procs);
    nkar_free(&ir->relocs);

    nk_free_t(ir->alloc, ir);
}

NkIrProc nkir_createProc(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    NkIrProc id{ir->procs.size};
    nkar_append(
        &ir->procs,
        (NkIrProc_T{
            .blocks{0, 0, 0, ir->alloc},
            .locals{0, 0, 0, ir->alloc},
        }));
    return id;
}

NkIrLabel nkir_createLabel(NkIrProg ir, nkid name) {
    NK_LOG_TRC("%s", __func__);

    NkIrLabel id{ir->blocks.size};
    nkar_append(
        &ir->blocks,
        (NkIrBlock{
            .name = name,
            .instrs{0, 0, 0, ir->alloc},
        }));
    return id;
}

void nkir_startProc(
    NkIrProg ir,
    NkIrProc _proc,
    nkid name,
    nktype_t proc_t,
    nkid_array arg_names,
    nkid file,
    size_t line,
    NkIrVisibility vis) {
    NK_LOG_TRC("%s", __func__);

    auto &proc = ir->procs.data[_proc.idx];

    proc.name = name;
    proc.proc_t = proc_t;
    nkav_copy(ir->alloc, &proc.arg_names, arg_names);
    proc.file = file;
    proc.start_line = line;
    proc.visibility = vis;

    nkir_activateProc(ir, _proc);
}

void nkir_activateProc(NkIrProg ir, NkIrProc _proc) {
    NK_LOG_TRC("%s", __func__);

    ir->cur_proc = _proc;
}

void nkir_finishProc(NkIrProg ir, NkIrProc _proc, size_t line) {
    NK_LOG_TRC("%s", __func__);

    auto &proc = ir->procs.data[_proc.idx];

    proc.end_line = line;
}

void *nkir_constGetData(NkIrProg ir, NkIrConst cnst) {
    NK_LOG_TRC("%s", __func__);

    return ir->consts.data[cnst.idx].data;
}

void *nkir_constRefDeref(NkIrProg ir, NkIrRef ref) {
    NK_LOG_TRC("%s", __func__);

    assert(ref.kind == NkIrRef_Rodata && "rodata ref expected");
    auto const &cnst = ir->consts.data[ref.index];
    auto data = (uint8_t *)cnst.data + ref.offset;
    int indir = ref.indir;
    while (indir--) {
        data = *(uint8_t **)data;
    }
    data += ref.post_offset;
    return data;
}

void nkir_gen(NkIrProg ir, NkIrInstrArray instrs_array) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto &proc = ir->procs.data[ir->cur_proc.idx];

    for (size_t i = 0; i < instrs_array.size; i++) {
        auto const &instr = instrs_array.data[i];

        if (instr.code == nkir_label) {
            proc.cur_block = instr.arg[1].id;
            nkar_append(&proc.blocks, proc.cur_block);
            continue;
        }

        assert(proc.cur_block < ir->blocks.size && "no current block");
        auto &block = ir->blocks.data[proc.cur_block].instrs;

        assert(
            instr.arg[0].kind != NkIrArg_Ref || instr.arg[0].ref.indir ||
            (instr.arg[0].ref.kind != NkIrRef_Rodata && instr.arg[0].ref.kind != NkIrRef_Arg));

        auto &instrs = ir->instrs;

        if (instr.code == nkir_ret && block.size && instrs.data[nkav_last(block)].code == nkir_ret) {
            continue;
        }

        size_t id = instrs.size;
        nkar_append(&instrs, instr);
        nkar_append(&block, id);
    }
}

void nkir_setLine(NkIrProg ir, size_t line) {
    ir->cur_line = line;
}

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, nkid name, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto &proc = ir->procs.data[ir->cur_proc.idx];

    NkIrLocalVar id{proc.locals.size};
    nkar_append(&proc.locals, (NkIrDecl_T{name, type}));
    return id;
}

NkIrGlobalVar nkir_makeGlobalVar(NkIrProg ir, nkid name, nktype_t type, NkIrVisibility vis) {
    NK_LOG_TRC("%s", __func__);

    NkIrGlobalVar id{ir->globals.size};
    nkar_append(&ir->globals, (NkIrGlobal_T{name, type, vis}));
    return id;
}

NkIrConst nkir_makeConst(NkIrProg ir, nkid name, void *data, nktype_t type, NkIrVisibility vis) {
    NK_LOG_TRC("%s", __func__);

    NkIrConst id{ir->consts.size};
    nkar_append(&ir->consts, (NkIrConst_T{name, data, type, vis}));
    return id;
}

NkIrExternData nkir_makeExternData(NkIrProg ir, nkid lib, nkid name, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternData id{ir->extern_data.size};
    nkar_append(&ir->extern_data, (NkIrExternSym_T{lib, name, type}));
    return id;
}

NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkid lib, nkid name, nktype_t proc_t) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternProc id{ir->extern_procs.size};
    nkar_append(&ir->extern_procs, (NkIrExternSym_T{lib, name, proc_t}));
    return id;
}

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
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

NkIrRef nkir_makeArgRef(NkIrProg ir, size_t index) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
    auto const &proc = ir->procs.data[ir->cur_proc.idx];

    auto const args_t = proc.proc_t->as.proc.info.args_t;
    assert(index < args_t.size && "arg index out of range");
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

    assert(ir->cur_proc.idx < ir->procs.size && "no current procedure");
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

NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrGlobalVar var) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = var.idx,
        .offset = 0,
        .post_offset = 0,
        .type = ir->globals.data[var.idx].type,
        .kind = NkIrRef_Data,
        .indir = 0,
    };
}

NkIrRef nkir_makeRodataRef(NkIrProg ir, NkIrConst cnst) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = cnst.idx,
        .offset = 0,
        .post_offset = 0,
        .type = ir->consts.data[cnst.idx].type,
        .kind = NkIrRef_Rodata,
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

    assert((ref.kind == NkIrRef_Data || ref.kind == NkIrRef_Rodata) && "invalid address reference");

    auto const id = ir->relocs.size;
    nkar_append(&ir->relocs, ref);

    return {
        .index = id,
        .offset = 0,
        .post_offset = 0,
        .type = ptr_t,
        .kind = NkIrRef_Address,
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

#define UNA_IR(NAME)                                                         \
    NkIrInstr CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef arg) { \
        NK_LOG_TRC("%s", __func__);                                          \
        return {{_arg(dst), _arg(arg), {}}, ir->cur_line, CAT(nkir_, NAME)}; \
    }
#define BIN_IR(NAME)                                                                      \
    NkIrInstr CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        NK_LOG_TRC("%s", __func__);                                                       \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, ir->cur_line, CAT(nkir_, NAME)};       \
    }
#define DBL_IR(NAME1, NAME2)                                                                                   \
    NkIrInstr CAT(nkir_make_, CAT(NAME1, CAT(_, NAME2)))(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        NK_LOG_TRC("%s", __func__);                                                                            \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, ir->cur_line, CAT(nkir_, CAT(NAME1, CAT(_, NAME2)))};       \
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

NkIrInstr nkir_make_comment(NkIrProg ir, nks comment) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(ir, comment), {}}, 0, nkir_comment};
}

bool nkir_write(NkArena *arena, NkIrProg ir, NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    nk_stream src{};
    bool res = nkcc_streamOpen(&src, conf);
    if (res) {
        nkir_translate2c(arena, ir, src);
        return !nkcc_streamClose(src);
    } else {
        // TODO Report errors to the user
        NK_LOG_ERR("Failed to run compiler `" nks_Fmt "`: %s", nks_Arg(conf.compiler_binary), nk_getLastErrorString());
        return false;
    }
}

#ifdef ENABLE_LOGGING
void nkir_inspectProgram(NkIrProg ir, NkStringBuilder *sb) {
    for (size_t i = 0; i < ir->procs.size; i++) {
        nkir_inspectProc(ir, {i}, sb);
    }

    nkir_inspectData(ir, sb);
    nkir_inspectExternSyms(ir, sb);
}

void nkir_inspectData(NkIrProg ir, NkStringBuilder *sb) {
    if (ir->globals.size) {
        for (size_t i = 0; i < ir->globals.size; i++) {
            auto const &decl = ir->globals.data[i];
            nksb_printf(sb, "\ndata ");
            if (decl.name != nk_invalid_id) {
                auto const global_name = nkid2s(decl.name);
                nksb_printf(sb, nks_Fmt, nks_Arg(global_name));
            } else {
                nksb_printf(sb, "global%" PRIu64, i);
            }
            nksb_printf(sb, ": ");
            nkirt_inspect(decl.type, sb);
        }
        nksb_printf(sb, "\n");
    }

    if (ir->consts.size) {
        bool printed = false;

        for (size_t i = 0; i < ir->consts.size; i++) {
            auto const &cnst = ir->consts.data[i];
            if (cnst.type->kind == NkType_Aggregate) {
                if (cnst.name != nk_invalid_id) {
                    auto const const_name = nkid2s(cnst.name);
                    nksb_printf(sb, "\nconst " nks_Fmt ": ", nks_Arg(const_name));
                } else {
                    nksb_printf(sb, "\nconst const%" PRIu64 ": ", i);
                }
                nkirt_inspect(cnst.type, sb);
                nksb_printf(sb, " = ");
                nkirv_inspect(cnst.data, cnst.type, sb);

                printed = true;
            }
        }
        if (printed) {
            nksb_printf(sb, "\n");
        }
    }
}

void inspectProcSignature(
    NkIrProcInfo const &proc_info,
    nkid_array arg_names,
    NkStringBuilder *sb,
    bool print_arg_names = true) {
    nksb_printf(sb, "(");

    for (size_t i = 0; i < proc_info.args_t.size; i++) {
        if (i) {
            nksb_printf(sb, ", ");
        }
        if (print_arg_names) {
            if (i < arg_names.size && arg_names.data[i] != nk_invalid_id) {
                auto const name = nkid2s(arg_names.data[i]);
                nksb_printf(sb, nks_Fmt ": ", nks_Arg(name));
            } else {
                nksb_printf(sb, "arg%" PRIu64 ": ", i);
            }
        }
        nkirt_inspect(proc_info.args_t.data[i], sb);
    }

    if (proc_info.flags & NkProcVariadic) {
        if (proc_info.args_t.size) {
            nksb_printf(sb, ", ");
        }
        nksb_printf(sb, "...");
    }

    nksb_printf(sb, ") ");

    nkirt_inspect(proc_info.ret_t, sb);
}

void nkir_inspectExternSyms(NkIrProg ir, NkStringBuilder *sb) {
    if (ir->extern_data.size) {
        for (auto const &data : nk_iterate(ir->extern_data)) {
            nksb_printf(sb, "\nextern");
            if (data.lib != nk_invalid_id) {
                nksb_printf(sb, " \"%s\"", nkid2cs(data.lib));
            }
            nksb_printf(sb, " data %s: ", nkid2cs(data.name));
            nkirt_inspect(data.type, sb);
        }
        nksb_printf(sb, "\n");
    }

    if (ir->extern_procs.size) {
        for (auto const &proc : nk_iterate(ir->extern_procs)) {
            nksb_printf(sb, "\nextern");
            if (proc.lib != nk_invalid_id) {
                nksb_printf(sb, " \"%s\"", nkid2cs(proc.lib));
            }
            nksb_printf(sb, " proc %s", nkid2cs(proc.name));
            inspectProcSignature(proc.type->as.proc.info, {}, sb, false);
        }
        nksb_printf(sb, "\n");
    }
}

void nkir_inspectProc(NkIrProg ir, NkIrProc _proc, NkStringBuilder *sb) {
    auto const &proc = ir->procs.data[_proc.idx];

    nksb_printf(
        sb,
        "\nproc%s %s",
        (proc.proc_t->as.proc.info.call_conv == NkCallConv_Cdecl ? " cdecl" : ""),
        nkid2cs(proc.name));
    inspectProcSignature(proc.proc_t->as.proc.info, proc.arg_names, sb);

    nksb_printf(sb, " {\n\n");

    if (proc.locals.size) {
        for (size_t i = 0; i < proc.locals.size; i++) {
            if (proc.locals.data[i].name != nk_invalid_id) {
                auto const local_name = nkid2s(proc.locals.data[i].name);
                nksb_printf(sb, nks_Fmt ": ", nks_Arg(local_name));
            } else {
                nksb_printf(sb, "var%" PRIu64 ": ", i);
            }
            nkirt_inspect(proc.locals.data[i].type, sb);
            nksb_printf(sb, "\n");
        }
        nksb_printf(sb, "\n");
    }

    size_t instr_index = 0;

    for (auto block_id : nk_iterate(proc.blocks)) {
        auto const &block = ir->blocks.data[block_id];

        nksb_printf(sb, "%s\n", nkid2cs(block.name));

        for (auto instr_id : nk_iterate(block.instrs)) {
            auto const &instr = ir->instrs.data[instr_id];

            if (instr.code == nkir_comment) {
                nksb_printf(sb, "%17s" nks_Fmt "\n", "// ", nks_Arg(instr.arg[1].comment));
                continue;
            }

            nksb_printf(sb, "%5zu%8s", instr_index++, nkirOpcodeName(instr.code));

            for (size_t i = 1; i < 3; i++) {
                auto const &arg = instr.arg[i];
                if (arg.kind != NkIrArg_None) {
                    nksb_printf(sb, ((i > 1) ? ", " : " "));
                }
                switch (arg.kind) {
                case NkIrArg_Ref: {
                    auto const &ref = arg.ref;
                    nkir_inspectRef(ir, _proc, ref, sb);
                    break;
                }
                case NkIrArg_RefArray: {
                    nksb_printf(sb, "(");
                    for (size_t i = 0; i < arg.refs.size; i++) {
                        if (i) {
                            nksb_printf(sb, ", ");
                        }
                        auto const &ref = arg.refs.data[i];
                        nkir_inspectRef(ir, _proc, ref, sb);
                    }
                    nksb_printf(sb, ")");
                    break;
                }
                case NkIrArg_Label:
                    if (arg.id < ir->blocks.size && ir->blocks.data[arg.id].name != nk_invalid_id) {
                        auto const name = nkid2s(ir->blocks.data[arg.id].name);
                        nksb_printf(sb, nks_Fmt, nks_Arg(name));
                    } else {
                        nksb_printf(sb, "(null)");
                    }
                    break;
                case NkIrArg_None:
                default:
                    break;
                }
            }

            if (instr.arg[0].kind == NkIrArg_Ref && instr.arg[0].ref.kind != NkIrRef_None) {
                nksb_printf(sb, " -> ");
                nkir_inspectRef(ir, _proc, instr.arg[0].ref, sb);
            }

            nksb_printf(sb, "\n");
        }

        nksb_printf(sb, "\n");
    }

    nksb_printf(sb, "}\n");
}

void nkir_inspectRef(NkIrProg ir, NkIrProc _proc, NkIrRef ref, NkStringBuilder *sb) {
    auto const &proc = ir->procs.data[_proc.idx];

    if (ref.kind == NkIrRef_None) {
        nksb_printf(sb, "{}");
        return;
    }
    for (size_t i = 0; i < ref.indir; i++) {
        nksb_printf(sb, "[");
    }
    switch (ref.kind) {
    case NkIrRef_Frame: {
        auto const &decl = proc.locals.data[ref.index];
        if (decl.name != nk_invalid_id) {
            auto const local_name = nkid2s(decl.name);
            nksb_printf(sb, nks_Fmt, nks_Arg(local_name));
        } else {
            nksb_printf(sb, "var%" PRIu64, ref.index);
        }
        break;
    }
    case NkIrRef_Arg: {
        if (ref.index < proc.arg_names.size && proc.arg_names.data[ref.index] != nk_invalid_id) {
            auto const name = nkid2s(proc.arg_names.data[ref.index]);
            nksb_printf(sb, nks_Fmt, nks_Arg(name));
        } else {
            nksb_printf(sb, "arg%" PRIu64, ref.index);
        }
        break;
    }
    case NkIrRef_Ret:
        nksb_printf(sb, "ret");
        break;
    case NkIrRef_Data: {
        auto const &decl = ir->globals.data[ref.index];
        if (decl.name != nk_invalid_id) {
            auto const name = nkid2s(decl.name);
            nksb_printf(sb, nks_Fmt, nks_Arg(name));
        } else {
            nksb_printf(sb, "global%" PRIu64, ref.index);
        }
        break;
    }
    case NkIrRef_Rodata:
        if (ref.type->kind == NkType_Numeric) {
            void *data = nkir_constRefDeref(ir, ref);
            nkirv_inspect(data, ref.type, sb);
        } else {
            auto const &cnst = ir->consts.data[ref.index];
            if (cnst.name != nk_invalid_id) {
                auto const const_name = nkid2s(cnst.name);
                nksb_printf(sb, nks_Fmt, nks_Arg(const_name));
            } else {
                nksb_printf(sb, "const%" PRIu64, ref.index);
            }
        }
        break;
    case NkIrRef_Proc: {
        auto const name = nkid2s(ir->procs.data[ref.index].name);
        nksb_printf(sb, nks_Fmt, nks_Arg(name));
        break;
    }
    case NkIrRef_ExternData: {
        auto const name = nkid2s(ir->extern_data.data[ref.index].name);
        nksb_printf(sb, nks_Fmt, nks_Arg(name));
        break;
    }
    case NkIrRef_ExternProc: {
        auto const name = nkid2s(ir->extern_procs.data[ref.index].name);
        nksb_printf(sb, nks_Fmt, nks_Arg(name));
        break;
    }
    case NkIrRef_Address: {
        nksb_printf(sb, "&");
        auto const &reloc_ref = ir->relocs.data[ref.index];
        nkir_inspectRef(ir, _proc, reloc_ref, sb);
        break;
    }
    case NkIrRef_None:
    default:
        assert(!"unreachable");
        break;
    }
    if (ref.offset) {
        nksb_printf(sb, "+%" PRIu64, ref.offset);
    }
    for (size_t i = 0; i < ref.indir; i++) {
        nksb_printf(sb, "]");
    }
    if (ref.post_offset) {
        nksb_printf(sb, "+%" PRIu64, ref.post_offset);
    }
    if (ref.kind != NkIrRef_Address) {
        nksb_printf(sb, ":");
        nkirt_inspect(ref.type, sb);
    }
}
#endif // ENABLE_LOGGIN
