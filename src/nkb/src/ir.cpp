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

NkIrArg _arg(NkIrRefArray args) {
    return {{.refs = args}, NkIrArg_RefArray};
}

} // namespace

char const *nkirOpcodeName(NkIrOpcode code) {
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

NkIrProg nkir_createProgram(NkAllocator alloc) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc(alloc, sizeof(NkIrProg_T))) NkIrProg_T{
        .alloc = alloc,

        .procs = decltype(NkIrProg_T::procs)::create(alloc),
        .blocks = decltype(NkIrProg_T::blocks)::create(alloc),
        .instrs = decltype(NkIrProg_T::instrs)::create(alloc),
        .globals = decltype(NkIrProg_T::globals)::create(alloc),
        .consts = decltype(NkIrProg_T::consts)::create(alloc),
        .extern_data = decltype(NkIrProg_T::extern_data)::create(alloc),
        .extern_procs = decltype(NkIrProg_T::extern_procs)::create(alloc),
    };
}

void nkir_freeProgram(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    ir->procs.deinit();
    ir->blocks.deinit();
    ir->instrs.deinit();
    ir->globals.deinit();
    ir->consts.deinit();

    nk_free(ir->alloc, ir, sizeof(NkIrProg_T));
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

NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkstr name, NkIrProcInfo proc_info) { // TODO
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
    auto &proc = ir->procs[ir->cur_proc.id];

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
    auto &proc = ir->procs[ir->cur_proc.id];

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
    auto &proc = ir->procs[ir->cur_proc.id];

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
        .type = nullptr, // TODO null proc ref type? Should be usize probably...
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

NkIrRef nkir_makeExternProcRef(NkIrProg, NkIrExternProc proc) {
    NK_LOG_TRC("%s", __func__);

    return {
        .index = proc.id,
        .offset = 0,
        .type = nullptr, // TODO Same thing here: null proc type
        .kind = NkIrRef_ExternProc,
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

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    NK_LOG_TRC("%s", __func__);
    return {{_arg(dst), _arg(proc), _arg(args)}, nkir_call};
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

void nkir_write(NkIrProg ir, NkbOutputKind kind, nkstr filepath) { // TODO
    NK_LOG_TRC("%s", __func__);
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir) { // TODO
    NK_LOG_TRC("%s", __func__);
}

void nkir_freeRunCtx(NkIrRunCtx ctx) { // TODO
    NK_LOG_TRC("%s", __func__);
}

void nkir_invoke(NkIrProc proc, NkIrPtrArray args, NkIrPtrArray ret) { // TODO
    NK_LOG_TRC("%s", __func__);
}

void nkir_inspectProgram(NkIrProg ir, NkStringBuilder sb) {
    nksb_printf(sb, "TODO nkir_inspectProgram not implemented");
}
