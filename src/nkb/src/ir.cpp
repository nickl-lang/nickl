#include "nkb/ir.h"

#include <algorithm>
#include <cassert>
#include <new>

#include "ir_impl.hpp"
#include "nk/common/logger.h"

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

void _inspectProcSignature(NkIrProcInfo const &proc_info, NkStringBuilder sb) {
    nksb_printf(sb, "(");

    for (size_t i = 0; i < proc_info.args_t.size; i++) {
        if (i) {
            nksb_printf(sb, ", ");
        }
        nksb_printf(sb, "arg%" PRIu64 ": ", i);
        nkt_inspect(proc_info.args_t.data[i], sb);
    }

    nksb_printf(sb, ")");

    for (size_t i = 0; i < proc_info.ret_t.size; i++) {
        if (i) {
            nksb_printf(sb, ",");
        }
        nksb_printf(sb, ": ");
        nkt_inspect(proc_info.ret_t.data[i], sb);
    }
}

} // namespace

char const *nkirOpcodeName(uint8_t code) {
    // TODO Compact with X macro
    switch (code) {
    case nkir_nop:
        return "nop";
    case nkir_ret:
        return "ret";
    case nkir_jmp:
        return "jmp";
    case nkir_jmpz:
        return "jmpz";
    case nkir_jmpnz:
        return "jmpnz";
    case nkir_ext:
        return "ext";
    case nkir_trunc:
        return "trunc";
    case nkir_fp2i:
        return "fp2i";
    case nkir_i2fp:
        return "i2fp";
    case nkir_call:
        return "call";
    case nkir_mov:
        return "mov";
    case nkir_lea:
        return "lea";
    case nkir_neg:
        return "neg";
    case nkir_add:
        return "add";
    case nkir_sub:
        return "sub";
    case nkir_mul:
        return "mul";
    case nkir_div:
        return "div";
    case nkir_mod:
        return "mod";
    case nkir_and:
        return "and";
    case nkir_or:
        return "or";
    case nkir_xor:
        return "xor";
    case nkir_lsh:
        return "lsh";
    case nkir_rsh:
        return "rsh";
    case nkir_cmp_eq:
        return "cmp eq";
    case nkir_cmp_ne:
        return "cmp ne";
    case nkir_cmp_lt:
        return "cmp lt";
    case nkir_cmp_le:
        return "cmp le";
    case nkir_cmp_gt:
        return "cmp gt";
    case nkir_cmp_ge:
        return "cmp ge";
    case nkir_label:
        return "label";
    default:
        return "";
    }
}

NkIrProg nkir_createProgram(NkAllocator alloc, nktype_t usize_t) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc_t<NkIrProg_T>(alloc)) NkIrProg_T{
        .usize_t = usize_t,
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

void nkir_startProc(NkIrProg ir, NkIrProc proc_id, nkstr name, NkIrProcInfo proc_info) {
    NK_LOG_TRC("%s", __func__);

    auto &proc = ir->procs[proc_id.id];

    proc.name = nk_strcpy(ir->alloc, name);
    proc.proc_info = proc_info;

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
    if (ref.is_indirect) {
        data = *(uint8_t **)data;
    }
    data += ref.offset;
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
            instr.arg[0].arg_kind != NkIrArg_Ref || instr.arg[0].ref.is_indirect ||
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

    NkIrExternData id{ir->consts.size()};
    ir->extern_data.emplace(NkIrExternData_T{
        .name = nk_strcpy(ir->alloc, name),
        .type = type,
    });
    return id;
}

NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkstr name, NkIrProcInfo proc_info) {
    NK_LOG_TRC("%s", __func__);

    NkIrExternProc id{ir->consts.size()};
    ir->extern_procs.emplace(NkIrExternProc_T{
        .name = nk_strcpy(ir->alloc, name),
        .proc_info = proc_info,
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
        .type = proc.locals[var.id],
        .kind = NkIrRef_Frame,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeArgRef(NkIrProg ir, size_t index) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto const &proc = ir->procs[ir->cur_proc.id];

    auto const args_t = proc.proc_info.args_t;
    assert(index < args_t.size && "arg index out of range");
    return {
        .index = index,
        .offset = 0,
        .type = args_t.data[index],
        .kind = NkIrRef_Arg,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRetRef(NkIrProg ir, size_t index) {
    NK_LOG_TRC("%s", __func__);

    assert(ir->cur_proc.id < ir->procs.size() && "no current procedure");
    auto const &proc = ir->procs[ir->cur_proc.id];

    auto const ret_t = proc.proc_info.ret_t;
    assert(ret_t.size == 1 && "Multiple return values not implemented");
    return {
        .index = 0,
        .offset = 0,
        .type = ret_t.data[0],
        .kind = NkIrRef_Ret,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrGlobalVar var) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = var.id,
        .offset = 0,
        .type = ir->globals[var.id],
        .kind = NkIrRef_Data,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRodataRef(NkIrProg ir, NkIrConst cnst) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = cnst.id,
        .offset = 0,
        .type = ir->consts[cnst.id].type,
        .kind = NkIrRef_Rodata,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.id,
        .offset = 0,
        .type = ir->usize_t,
        .kind = NkIrRef_Proc,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData data) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = data.id,
        .offset = 0,
        .type = ir->extern_data[data.id].type,
        .kind = NkIrRef_ExternData,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.id,
        .offset = 0,
        .type = ir->usize_t,
        .kind = NkIrRef_ExternProc,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeAddressRef(NkIrProg ir, NkIrRef ref) {
    NK_LOG_TRC("%s", __func__);

    assert((ref.kind == NkIrRef_Data || ref.kind == NkIrRef_Rodata) && "invalid address reference");

    auto const id = ir->relocs.size();
    ir->relocs.emplace(ref);

    return {
        .index = id,
        .offset = 0,
        .type = ir->usize_t,
        .kind = NkIrRef_Reloc,
        .is_indirect = false,
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

NkIrInstr nkir_make_ext(NkIrRef dst, NkIrRef src) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(src)}, nkir_ext};
}

NkIrInstr nkir_make_trunc(NkIrRef dst, NkIrRef src) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(src)}, nkir_trunc};
}

NkIrInstr nkir_make_fp2i(NkIrRef dst, NkIrRef src) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(src)}, nkir_fp2i};
}

NkIrInstr nkir_make_i2fp(NkIrRef dst, NkIrRef src) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(src)}, nkir_i2fp};
}

NkIrInstr nkir_make_call(NkIrProg ir, NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(proc), _arg(ir, args)}, nkir_call};
}

NkIrInstr nkir_make_mov(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_mov};
}

NkIrInstr nkir_make_lea(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_lea};
}

NkIrInstr nkir_make_neg(NkIrRef dst, NkIrRef arg) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(arg), {}}, nkir_neg};
}

NkIrInstr nkir_make_add(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_add};
}

NkIrInstr nkir_make_sub(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_sub};
}

NkIrInstr nkir_make_mul(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_mul};
}

NkIrInstr nkir_make_div(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_div};
}

NkIrInstr nkir_make_mod(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_mod};
}

NkIrInstr nkir_make_and(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_and};
}

NkIrInstr nkir_make_or(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_or};
}

NkIrInstr nkir_make_xor(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_xor};
}

NkIrInstr nkir_make_lsh(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_lsh};
}

NkIrInstr nkir_make_rsh(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_rsh};
}

NkIrInstr nkir_make_cmp_eq(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_cmp_eq};
}

NkIrInstr nkir_make_cmp_ne(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_cmp_ne};
}

NkIrInstr nkir_make_cmp_lt(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_cmp_lt};
}

NkIrInstr nkir_make_cmp_le(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_cmp_le};
}

NkIrInstr nkir_make_cmp_gt(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_cmp_gt};
}

NkIrInstr nkir_make_cmp_ge(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_cmp_ge};
}

NkIrInstr nkir_make_label(NkIrLabel label) {
    NK_LOG_TRC("%s", __func__);
    return {{{}, _arg(label), {}}, nkir_label};
}

bool nkir_write(NkIrProg ir, NkbOutputKind kind, nkstr out_file) { // TODO
    NK_LOG_TRC("%s", __func__);

    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nksb_printf(sb, "gcc -x c -O2 -o %.*s -", (int)out_file.size, out_file.data);
    auto compile_cmd = nksb_concat(sb);

    auto pipe = popen(compile_cmd.data, "w");
    fprintf(pipe, R"(
        #include <stdio.h>
        int main(int argc, char** argv) {
            puts("Hello, World!");
            return 0;
        }
    )");
    pclose(pipe);

    return true;
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc_t<NkIrRunCtx_T>(ir->alloc)) NkIrRunCtx_T{
        .ir = ir,
    };
}

void nkir_freeRunCtx(NkIrRunCtx ctx) {
    NK_LOG_TRC("%s", __func__);

    nk_free_t(ctx->ir->alloc, ctx);
}

void nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, NkIrPtrArray args, NkIrPtrArray ret) { // TODO
    NK_LOG_TRC("%s", __func__);

    puts("Hello, World!");
}

void nkir_inspectProgram(NkIrProg ir, NkStringBuilder sb) {
    for (size_t i = 0; i < ir->procs.size(); i++) {
        nkir_inspectProc(ir, {i}, sb);
    }

    nkir_inspectData(ir, sb);
    nkir_inspectExternSyms(ir, sb);
}

void nkir_inspectData(NkIrProg ir, NkStringBuilder sb) {
    if (ir->globals.size()) {
        for (size_t i = 0; i < ir->globals.size(); i++) {
            nksb_printf(sb, "\ndata global%" PRIu64 ": ", i);
            nkt_inspect(ir->globals[i], sb);
        }
        nksb_printf(sb, "\n");
    }

    if (ir->consts.size()) {
        for (size_t i = 0; i < ir->consts.size(); i++) {
            auto const &cnst = ir->consts[i];
            if (cnst.type->kind == NkType_Aggregate) {
                nksb_printf(sb, "\nconst const%" PRIu64 ": ", i);
                nkt_inspect(cnst.type, sb);
                nksb_printf(sb, " = ");
                nkval_inspect(cnst.data, cnst.type, sb);
            }
        }
        nksb_printf(sb, "\n");
    }
}

void nkir_inspectExternSyms(NkIrProg ir, NkStringBuilder sb) {
    if (ir->extern_data.size()) {
        for (auto const &data : ir->extern_data) {
            nksb_printf(sb, "\nextern data %.*s: ", (int)data.name.size, data.name.data);
            nkt_inspect(data.type, sb);
        }
        nksb_printf(sb, "\n");
    }

    if (ir->extern_procs.size()) {
        for (auto const &proc : ir->extern_procs) {
            nksb_printf(sb, "\nextern proc %.*s", (int)proc.name.size, proc.name.data);
            _inspectProcSignature(proc.proc_info, sb);
        }
        nksb_printf(sb, "\n");
    }
}

void nkir_inspectProc(NkIrProg ir, NkIrProc proc_id, NkStringBuilder sb) {
    auto const &proc = ir->procs[proc_id.id];

    nksb_printf(sb, "proc %.*s", (int)proc.name.size, proc.name.data);
    _inspectProcSignature(proc.proc_info, sb);

    nksb_printf(sb, " {\n\n");

    if (!proc.locals.empty()) {
        for (size_t i = 0; i < proc.locals.size(); i++) {
            nksb_printf(sb, "var%" PRIu64 ": ", i);
            nkt_inspect(proc.locals[i], sb);
            nksb_printf(sb, "\n");
        }
        nksb_printf(sb, "\n");
    }

    for (auto block_id : proc.blocks) {
        auto const &block = ir->blocks[block_id];

        nksb_printf(sb, "%.*s\n", (int)block.name.size, block.name.data);

        for (auto instr_id : block.instrs) {
            auto const &instr = ir->instrs[instr_id];

            nksb_printf(sb, "  %s", nkirOpcodeName(instr.code));

            for (size_t i = 1; i < 3; i++) {
                auto const &arg = instr.arg[i];
                if (arg.arg_kind != NkIrArg_None) {
                    nksb_printf(sb, ((i > 1) ? ", " : " "));
                }
                switch (arg.arg_kind) {
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

            if (instr.arg[0].arg_kind == NkIrArg_Ref && instr.arg[0].ref.kind != NkIrRef_None) {
                nksb_printf(sb, " -> ");
                nkir_inspectRef(ir, instr.arg[0].ref, sb);
            }

            nksb_printf(sb, "\n");
        }

        nksb_printf(sb, "\n");
    }

    nksb_printf(sb, "}\n");
}

void nkir_inspectRef(NkIrProg ir, NkIrRef ref, NkStringBuilder sb) {
    if (ref.kind == NkIrRef_None) {
        nksb_printf(sb, "{}");
        return;
    }
    if (ref.is_indirect) {
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
        if (ref.type->kind == NkType_Basic) {
            void *data = nkir_constRefDeref(ir, ref);
            nkval_inspect(data, ref.type, sb);
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
    case NkIrRef_Reloc: {
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
    if (ref.is_indirect) {
        nksb_printf(sb, "]");
    }
    if (ref.offset) {
        nksb_printf(sb, "+%" PRIu64 "", ref.offset);
    }
    if (ref.kind != NkIrRef_Reloc) {
        nksb_printf(sb, ":");
        nkt_inspect(ref.type, sb);
    }
}
