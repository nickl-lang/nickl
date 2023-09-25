#include "nkb/ir.h"

#include <algorithm>
#include <cassert>
#include <new>

#include "ir_impl.hpp"
#include "nk/common/logger.h"
#include "nk/sys/file.h"
#include "nk/sys/process.h"

namespace {

NK_LOG_USE_SCOPE(ir);

NkIrArg _arg(NkIrRef ref) {
    return {{.ref = ref}, NkIrArg_Ref};
}

NkIrArg _arg(NkIrLabel label) {
    return {{.id = label.id}, NkIrArg_Label};
}

NkIrArg _arg(NkIrProg ir, NkIrRefArray args) {
    auto refs = nk_alloc_t<NkIrRef>(ir->alloc, args.size);
    memcpy(refs, args.data, args.size * sizeof(NkIrRef));
    return {{.refs{refs, args.size}}, NkIrArg_RefArray};
}

#ifdef ENABLE_LOGGING
void inspectProcSignature(NkIrProcInfo const &proc_info, NkStringBuilder *sb, bool print_arg_names = true) {
    nksb_printf(sb, "(");

    for (size_t i = 0; i < proc_info.args_t.size; i++) {
        if (i) {
            nksb_printf(sb, ", ");
        }
        if (print_arg_names) {
            nksb_printf(sb, "arg%" PRIu64 ": ", i);
        }
        nkirt_inspect(proc_info.args_t.data[i], sb);
    }

    if (proc_info.flags & NkProcVariadic) {
        if (proc_info.args_t.size) {
            nksb_printf(sb, ", ");
        }
        nksb_printf(sb, "...");
    }

    nksb_printf(sb, ")");

    for (size_t i = 0; i < proc_info.ret_t.size; i++) {
        if (i) {
            nksb_printf(sb, ",");
        }
        nksb_printf(sb, " ");
        nkirt_inspect(proc_info.ret_t.data[i], sb);
    }
}
#endif // ENABLE_LOGGING

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

        .procs = decltype(NkIrProg_T::procs)::create(alloc),
        .blocks = decltype(NkIrProg_T::blocks)::create(alloc),
        .instrs = decltype(NkIrProg_T::instrs)::create(alloc),
        .globals = decltype(NkIrProg_T::globals)::create(alloc),
        .consts = decltype(NkIrProg_T::consts)::create(alloc),
        .extern_data = decltype(NkIrProg_T::extern_data)::create(alloc),
        .extern_procs = decltype(NkIrProg_T::extern_procs)::create(alloc),
        .relocs = decltype(NkIrProg_T::relocs)::create(alloc),
    };
}

void nkir_freeProgram(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    ir->procs.deinit();
    ir->blocks.deinit();
    ir->instrs.deinit();
    ir->globals.deinit();
    ir->consts.deinit();

    nk_free_t(ir->alloc, ir);
}

NkIrProc nkir_createProc(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    NkIrProc id{ir->procs.size()};
    ir->procs.emplace(NkIrProc_T{
        .blocks = decltype(NkIrProc_T::blocks)::create(ir->alloc),
        .locals = decltype(NkIrProc_T::locals)::create(ir->alloc),
    });
    return id;
}

NkIrLabel nkir_createLabel(NkIrProg ir, nkstr name) {
    NK_LOG_TRC("%s", __func__);

    NkIrLabel id{ir->blocks.size()};
    ir->blocks.emplace(NkIrBlock{
        .name = nk_strcpy(ir->alloc, name),
        .instrs = decltype(NkIrBlock::instrs)::create(ir->alloc),
    });
    return id;
}

void nkir_startProc(NkIrProg ir, NkIrProc proc_id, nkstr name, nktype_t proc_t) {
    NK_LOG_TRC("%s", __func__);

    auto &proc = ir->procs[proc_id.id];

    proc.name = nk_strcpy(ir->alloc, name);
    proc.proc_t = proc_t;

    nkir_activateProc(ir, proc_id);
}

void nkir_activateProc(NkIrProg ir, NkIrProc proc_id) {
    NK_LOG_TRC("%s", __func__);

    ir->cur_proc = proc_id;
}

void *nkir_constGetData(NkIrProg ir, NkIrConst cnst) {
    NK_LOG_TRC("%s", __func__);

    return ir->consts[cnst.id].data;
}

void *nkir_constRefDeref(NkIrProg ir, NkIrRef ref) {
    NK_LOG_TRC("%s", __func__);

    assert(ref.kind == NkIrRef_Rodata && "rodata ref expected");
    auto const &cnst = ir->consts[ref.index];
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

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto &proc = ir->procs[ir->cur_proc.id];

    for (size_t i = 0; i < instrs_array.size; i++) {
        auto const &instr = instrs_array.data[i];

        if (instr.code == nkir_label) {
            proc.cur_block = instr.arg[1].id;
            proc.blocks.emplace(proc.cur_block);
            continue;
        }

        assert(proc.cur_block < ir->blocks.size() && "no current block");
        auto &block = ir->blocks[proc.cur_block].instrs;

        assert(
            instr.arg[0].kind != NkIrArg_Ref || instr.arg[0].ref.indir ||
            (instr.arg[0].ref.kind != NkIrRef_Rodata && instr.arg[0].ref.kind != NkIrRef_Arg));

        auto &instrs = ir->instrs;

        if (instr.code == nkir_ret && block.size() && instrs[block.back()].code == nkir_ret) {
            continue;
        }

        size_t id = instrs.size();
        instrs.emplace(instr);
        block.emplace(id);
    }
}

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto &proc = ir->procs[ir->cur_proc.id];

    NkIrLocalVar id{proc.locals.size()};
    proc.locals.emplace(type);
    return id;
}

NkIrGlobalVar nkir_makeGlobalVar(NkIrProg ir, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    NkIrGlobalVar id{ir->globals.size()};
    ir->globals.emplace(type);
    return id;
}

NkIrConst nkir_makeConst(NkIrProg ir, void *data, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    NkIrConst id{ir->consts.size()};
    ir->consts.emplace(NkIrConst_T{
        .data = data,
        .type = type,
    });
    return id;
}

NkIrExternData nkir_makeExternData(NkIrProg ir, nkstr name, nktype_t type) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternData id{ir->extern_data.size()};
    ir->extern_data.emplace(NkIrExternData_T{
        .name = nk_strcpy(ir->alloc, name),
        .type = type,
    });
    return id;
}

NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkstr name, nktype_t proc_t) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternProc id{ir->extern_procs.size()};
    ir->extern_procs.emplace(NkIrExternProc_T{
        .name = nk_strcpy(ir->alloc, name),
        .proc_t = proc_t,
    });
    return id;
}

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto const &proc = ir->procs[ir->cur_proc.id];

    return {
        .index = var.id,
        .offset = 0,
        .post_offset = 0,
        .type = proc.locals[var.id],
        .kind = NkIrRef_Frame,
        .indir = 0,
    };
}

NkIrRef nkir_makeArgRef(NkIrProg ir, size_t index) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto const &proc = ir->procs[ir->cur_proc.id];

    auto const args_t = proc.proc_t->as.proc.info.args_t;
    assert(index < args_t.size && "arg index out of range");
    return {
        .index = index,
        .offset = 0,
        .post_offset = 0,
        .type = args_t.data[index],
        .kind = NkIrRef_Arg,
        .indir = 1,
    };
}

NkIrRef nkir_makeRetRef(NkIrProg ir, size_t index) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto const &proc = ir->procs[ir->cur_proc.id];

    auto const ret_t = proc.proc_t->as.proc.info.ret_t;
    assert(ret_t.size == 1 && "Multiple return values not implemented");
    return {
        .index = 0,
        .offset = 0,
        .post_offset = 0,
        .type = ret_t.data[0],
        .kind = NkIrRef_Ret,
        .indir = 1,
    };
}

NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrGlobalVar var) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = var.id,
        .offset = 0,
        .post_offset = 0,
        .type = ir->globals[var.id],
        .kind = NkIrRef_Data,
        .indir = 0,
    };
}

NkIrRef nkir_makeRodataRef(NkIrProg ir, NkIrConst cnst) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = cnst.id,
        .offset = 0,
        .post_offset = 0,
        .type = ir->consts[cnst.id].type,
        .kind = NkIrRef_Rodata,
        .indir = 0,
    };
}

NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.id,
        .offset = 0,
        .post_offset = 0,
        .type = ir->procs[proc.id].proc_t,
        .kind = NkIrRef_Proc,
        .indir = 0,
    };
}

NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData data) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = data.id,
        .offset = 0,
        .post_offset = 0,
        .type = ir->extern_data[data.id].type,
        .kind = NkIrRef_ExternData,
        .indir = 0,
    };
}

NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.id,
        .offset = 0,
        .post_offset = 0,
        .type = ir->extern_procs[proc.id].proc_t,
        .kind = NkIrRef_ExternProc,
        .indir = 0,
    };
}

NkIrRef nkir_makeAddressRef(NkIrProg ir, NkIrRef ref, nktype_t ptr_t) {
    NK_LOG_TRC("%s", __func__);

    assert((ref.kind == NkIrRef_Data || ref.kind == NkIrRef_Rodata) && "invalid address reference");

    auto const id = ir->relocs.size();
    ir->relocs.emplace(ref);

    return {
        .index = id,
        .offset = 0,
        .post_offset = 0,
        .type = ptr_t,
        .kind = NkIrRef_Address,
        .indir = 0,
    };
}

NkIrInstr nkir_make_nop() {
    NK_LOG_TRC("%s", __func__);
    return {{}, nkir_nop};
}

NkIrInstr nkir_make_ret() {
    NK_LOG_TRC("%s", __func__);
    return {{}, nkir_ret};
}

NkIrInstr nkir_make_jmp(NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(label)}, nkir_jmp};
}

NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpz};
}

NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpnz};
}

NkIrInstr nkir_make_call(NkIrProg ir, NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(proc), _arg(ir, args)}, nkir_call};
}

NkIrInstr nkir_make_label(NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(label), {}}, nkir_label};
}

#define UNA_IR(NAME)                                            \
    NkIrInstr CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef arg) { \
        NK_LOG_TRC("%s", __func__);                             \
        return {{_arg(dst), _arg(arg), {}}, CAT(nkir_, NAME)};  \
    }
#define BIN_IR(NAME)                                                         \
    NkIrInstr CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        NK_LOG_TRC("%s", __func__);                                          \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, CAT(nkir_, NAME)};        \
    }
#define DBL_IR(NAME1, NAME2)                                                                      \
    NkIrInstr CAT(nkir_make_, CAT(NAME1, CAT(_, NAME2)))(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        NK_LOG_TRC("%s", __func__);                                                               \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, CAT(nkir_, CAT(NAME1, CAT(_, NAME2)))};        \
    }
#include "nkb/ir.inl"

bool nkir_write(NkIrProg ir, NkbOutputKind kind, nkstr out_file) { // TODO
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    nksb_printf(&sb, "gcc -x c -O2 -o %.*s -", (int)out_file.size, out_file.data);

    auto in = nk_createPipe();
    nkpid_t pid = 0;
    nk_execAsync(sb.data, &pid, &in, nullptr);
    char src[] = R"(
        #include <stdio.h>
        int main(int argc, char** argv) {
            puts("Hello, World!");
            return 0;
        }
    )";
    nk_write(in.write, src, sizeof(src) - 1);
    nk_close(in.write);
    nk_waitpid(pid, nullptr);

    return true;
}

#ifdef ENABLE_LOGGING
void nkir_inspectProgram(NkIrProg ir, NkStringBuilder *sb) {
    for (size_t i = 0; i < ir->procs.size(); i++) {
        nkir_inspectProc(ir, {i}, sb);
    }

    nkir_inspectData(ir, sb);
    nkir_inspectExternSyms(ir, sb);
}

void nkir_inspectData(NkIrProg ir, NkStringBuilder *sb) {
    if (ir->globals.size()) {
        for (size_t i = 0; i < ir->globals.size(); i++) {
            nksb_printf(sb, "\ndata global%" PRIu64 ": ", i);
            nkirt_inspect(ir->globals[i], sb);
        }
        nksb_printf(sb, "\n");
    }

    if (ir->consts.size()) {
        bool printed = false;

        for (size_t i = 0; i < ir->consts.size(); i++) {
            auto const &cnst = ir->consts[i];
            if (cnst.type->kind == NkType_Aggregate) {
                nksb_printf(sb, "\nconst const%" PRIu64 ": ", i);
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

void nkir_inspectExternSyms(NkIrProg ir, NkStringBuilder *sb) {
    if (ir->extern_data.size()) {
        for (auto const &data : ir->extern_data) {
            nksb_printf(sb, "\nextern data %.*s: ", (int)data.name.size, data.name.data);
            nkirt_inspect(data.type, sb);
        }
        nksb_printf(sb, "\n");
    }

    if (ir->extern_procs.size()) {
        for (auto const &proc : ir->extern_procs) {
            nksb_printf(sb, "\nextern proc %.*s", (int)proc.name.size, proc.name.data);
            inspectProcSignature(proc.proc_t->as.proc.info, sb, false);
        }
        nksb_printf(sb, "\n");
    }
}

void nkir_inspectProc(NkIrProg ir, NkIrProc proc_id, NkStringBuilder *sb) {
    auto const &proc = ir->procs[proc_id.id];

    nksb_printf(
        sb,
        "\nproc%s %.*s",
        (proc.proc_t->as.proc.info.call_conv == NkCallConv_Cdecl ? " cdecl" : ""),
        (int)proc.name.size,
        proc.name.data);
    inspectProcSignature(proc.proc_t->as.proc.info, sb);

    nksb_printf(sb, " {\n\n");

    if (!proc.locals.empty()) {
        for (size_t i = 0; i < proc.locals.size(); i++) {
            nksb_printf(sb, "var%" PRIu64 ": ", i);
            nkirt_inspect(proc.locals[i], sb);
            nksb_printf(sb, "\n");
        }
        nksb_printf(sb, "\n");
    }

    size_t instr_index = 0;

    for (auto block_id : proc.blocks) {
        auto const &block = ir->blocks[block_id];

        nksb_printf(sb, "%.*s\n", (int)block.name.size, block.name.data);

        for (auto instr_id : block.instrs) {
            auto const &instr = ir->instrs[instr_id];

            nksb_printf(sb, "%5zu%8s", instr_index++, nkirOpcodeName(instr.code));

            for (size_t i = 1; i < 3; i++) {
                auto const &arg = instr.arg[i];
                if (arg.kind != NkIrArg_None) {
                    nksb_printf(sb, ((i > 1) ? ", " : " "));
                }
                switch (arg.kind) {
                case NkIrArg_Ref: {
                    auto const &ref = arg.ref;
                    nkir_inspectRef(ir, ref, sb);
                    break;
                }
                case NkIrArg_RefArray: {
                    nksb_printf(sb, "(");
                    for (size_t i = 0; i < arg.refs.size; i++) {
                        if (i) {
                            nksb_printf(sb, ", ");
                        }
                        auto const &ref = arg.refs.data[i];
                        nkir_inspectRef(ir, ref, sb);
                    }
                    nksb_printf(sb, ")");
                    break;
                }
                case NkIrArg_Label:
                    if (arg.id < ir->blocks.size() && ir->blocks[arg.id].name.size != 0) {
                        nksb_printf(sb, "%.*s", (int)ir->blocks[arg.id].name.size, ir->blocks[arg.id].name.data);
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
                nkir_inspectRef(ir, instr.arg[0].ref, sb);
            }

            nksb_printf(sb, "\n");
        }

        nksb_printf(sb, "\n");
    }

    nksb_printf(sb, "}\n");
}

void nkir_inspectRef(NkIrProg ir, NkIrRef ref, NkStringBuilder *sb) {
    if (ref.kind == NkIrRef_None) {
        nksb_printf(sb, "{}");
        return;
    }
    for (size_t i = 0; i < ref.indir; i++) {
        nksb_printf(sb, "[");
    }
    switch (ref.kind) {
    case NkIrRef_Frame:
        nksb_printf(sb, "var%" PRIu64 "", ref.index);
        break;
    case NkIrRef_Arg:
        nksb_printf(sb, "arg%" PRIu64 "", ref.index);
        break;
    case NkIrRef_Ret:
        nksb_printf(sb, "ret");
        break;
    case NkIrRef_Data:
        nksb_printf(sb, "global%" PRIu64 "", ref.index);
        break;
    case NkIrRef_Rodata:
        if (ref.type->kind == NkType_Numeric) {
            void *data = nkir_constRefDeref(ir, ref);
            nkirv_inspect(data, ref.type, sb);
        } else {
            nksb_printf(sb, "const%" PRIu64 "", ref.index);
        }
        break;
    case NkIrRef_Proc: {
        auto const name = ir->procs[ref.index].name;
        nksb_printf(sb, "%.*s", (int)name.size, name.data);
        break;
    }
    case NkIrRef_ExternData: {
        auto const name = ir->extern_data[ref.index].name;
        nksb_printf(sb, "%.*s", (int)name.size, name.data);
        break;
    }
    case NkIrRef_ExternProc: {
        auto const name = ir->extern_procs[ref.index].name;
        nksb_printf(sb, "%.*s", (int)name.size, name.data);
        break;
    }
    case NkIrRef_Address: {
        nksb_printf(sb, "&");
        auto const &reloc_ref = ir->relocs[ref.index];
        nkir_inspectRef(ir, reloc_ref, sb);
        break;
    }
    case NkIrRef_None:
    default:
        assert(!"unreachable");
        break;
    }
    if (ref.offset) {
        nksb_printf(sb, "+%" PRIu64 "", ref.offset);
    }
    for (size_t i = 0; i < ref.indir; i++) {
        nksb_printf(sb, "]");
    }
    if (ref.post_offset) {
        nksb_printf(sb, "+%" PRIu64 "", ref.post_offset);
    }
    if (ref.kind != NkIrRef_Address) {
        nksb_printf(sb, ":");
        nkirt_inspect(ref.type, sb);
    }
}
#endif // ENABLE_LOGGING
