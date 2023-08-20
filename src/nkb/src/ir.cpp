#include "nkb/ir.h"

#include <algorithm>
#include <cassert>
#include <new>

#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nkb/common.h"
// #include "nkb/value.h"

// char const *s_nk_ir_names[] = {
// #define X(NAME) #NAME,
// #include "nkb/ir.inl"
// };

namespace {

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
}

NkIrProg nkir_createProgram() {
    return new (nk_alloc(nk_default_allocator, sizeof(NkIrProg_T))) NkIrProg_T{};
}

void nkir_freeProgram(NkIrProg ir) {
    // nkbc_free(ir->bc);
    // for (auto proc : ir->procs) {
    //     proc->~NkIrProc_T();
    //     nk_free(nk_default_allocator, proc, sizeof(NkIrProc_T));
    // }
    ir->~NkIrProg_T();
    nk_free(nk_default_allocator, ir, sizeof(NkIrProg_T));
}

NkIrProc nkir_createProc(NkIrProg ir) {
    // return ir->procs.emplace_back(new (nk_alloc(nk_default_allocator, sizeof(NkIrProc_T))) NkIrProc_T{
    //     .ir = ir,
    //     .proc_t{},
    // });
}

NkIrLabel nkir_createLabel(NkIrProg ir) {
    NkIrLabel id{ir->blocks.size()};
    ir->blocks.emplace_back(NkIrBlock{});
    return id;
}

void nkir_startProc(NkIrProc proc, nkstr name, NkIrProcInfo proc_info) {
    // proc->name = std_str(name);

    // proc->proc_t = proc_t;
    // proc->state = NkIrProc_Complete;

    // nkir_activateProc(proc->ir, proc);
}

void nkir_activateProc(NkIrProg ir, NkIrProc proc) {
    // ir->cur_proc = proc;
}

void *nkir_constGetData(NkIrProg ir, NkIrConst cnst) {
    // assert(cnst.id < ir->consts.size() && "invalid const");
    // return ir->consts[cnst.id];
}

void *nkir_constRefDeref(NkIrProg ir, NkIrRef ref) {
    // assert(ref.kind == NkIrRef_Const && "const ref expected");
    // auto const val = nkir_constGetValue(ir, {ref.index});
    // auto type = nkval_typeof(val);
    // auto data = (uint8_t *)nkval_data(val) + ref.offset;
    // if (ref.is_indirect) {
    //     assert(nkt_typeclassid(type) == NkType_Ptr);
    //     type = type->as.ptr.target_type;
    //     data = *(uint8_t **)data;
    // }
    // data += ref.post_offset;
    // return {data, type};
}

void nkir_gen(NkIrProg ir, NkIrInstrArray instrs) {
    // assert(ir->cur_proc && "no current procedure");
    // assert(ir->cur_proc->cur_block < ir->blocks.size() && "no current block");
    // assert(
    //     instr.arg[0].arg_type != NkIrArg_Ref || instr.arg[0].ref.is_indirect ||
    //     (instr.arg[0].ref.kind != NkIrRef_Const && instr.arg[0].ref.kind !=
    //     NkIrRef_Arg));

    // auto &instrs = ir->instrs;
    // auto &block = ir->blocks[ir->cur_proc->cur_block].instrs;

    // if (instr.code == nkir_ret && block.size() && instrs[block.back()].code == nkir_ret) {
    //     return;
    // }

    // size_t id = instrs.size();
    // instrs.emplace_back(instr);
    // block.emplace_back(id);
}

// void nkir_startBlock(NkIrProg ir, NkIrLabel block_id, nkstr name) {
//     assert(ir->cur_proc && "no current procedure");
//     assert(block_id.id < ir->blocks.size() && "invalid block");

//     auto &block = ir->blocks[block_id.id];
//     block.name = std_str(name);

//     ir->cur_proc->blocks.emplace_back(block_id.id);

//     assert(ir->cur_proc && "no current procedure");
//     assert(block_id.id < ir->blocks.size() && "invalid block");
//     ir->cur_proc->cur_block = block_id.id;
// }

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, nktype_t type) {
    // assert(ir->cur_proc && "no current procedure");
    // NkIrLocalVar id{ir->cur_proc->locals.size()};
    // ir->cur_proc->locals.emplace_back(type);
    // return id;
}

NkIrGlobalVar nkir_makeGlobalVar(NkIrProg ir, nktype_t type) {
    NkIrGlobalVar id{ir->globals.size()};
    ir->globals.emplace_back(type);
    return id;
}

NkIrConst nkir_makeConst(NkIrProg ir, void *data, nktype_t type) {
    // NkIrConst id{ir->consts.size()};
    // ir->consts.emplace_back(val);
    // return id;
}

NkIrExternData nkir_makeExternData(NkIrProg ir, nkstr name, nktype_t type) {
}

NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkstr name, NkIrProcInfo proc_info) {
}

// NkIrExtSymId nkir_makeExtSym(NkIrProg ir, NkIrExternLib lib, nkstr name, nktype_t type) {
//     NkIrExtSymId id{ir->exsyms.size()};
//     ir->exsyms.emplace_back(IrExSym{
//         .name = std_str(name),
//         .so_id = so,
//         .type = type,
//     });
//     return id;
// }

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var) {
    // assert(ir->cur_proc && "no current procedure");
    // return {
    //     .index = var.id,
    //     .offset = 0,
    //     .type = ir->cur_proc->locals[var.id],
    //     .kind = NkIrRef_Frame,
    //     .is_indirect = false,
    // };
}

NkIrRef nkir_makeArgRef(NkIrProg ir, size_t index) {
    // assert(ir->cur_proc && "no current procedure");
    // assert(
    //     (ir->cur_proc->state == NkIrProc_Complete || ir->cur_proc->proc_info.args_t.) &&
    //     "referencing incomplete procedure args type");
    // auto const args_t = ir->cur_proc->state == NkIrProc_Complete ? ir->cur_proc->proc_t->as.proc.args_t
    //                                                              : ir->cur_proc->proc_info.args_t;
    // assert(index < args_t->as.tuple.elems.size && "arg index out of range");
    // return {
    //     .index = index,
    //     .offset = 0,
    //     .type = args_t->as.tuple.elems.data[index].type,
    //     .kind = NkIrRef_Arg,
    //     .is_indirect = false,
    // };
}

NkIrRef nkir_makeRetRef(NkIrProg ir, size_t index) {
    // assert(ir->cur_proc && "no current procedure");
    // assert(
    //     (ir->cur_proc->state == NkIrProc_Complete || ir->cur_proc->proc_info.ret_t) &&
    //     "referencing incomplete procedure ret type");
    // auto const ret_t =
    //     ir->cur_proc->state == NkIrProc_Complete ? ir->cur_proc->proc_t->as.proc.ret_t :
    //     ir->cur_proc->proc_info.ret_t;
    // return {
    //     .data = {},
    //     .offset = 0,
    //     .type = ret_t,
    //     .kind = NkIrRef_Ret,
    //     .is_indirect = false,
    // };
}

NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrGlobalVar var) {
    return {
        .index = var.id,
        .offset = 0,
        .type = ir->globals[var.id],
        .kind = NkIrRef_Data,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRodataRef(NkIrProg ir, NkIrConst cnst) {
    // return {
    //     .index = cnst.id,
    //     .offset = 0,
    //     .type = nkval_typeof(ir->consts[cnst.id]),
    //     .kind = NkIrRef_Rodata,
    //     .is_indirect = false,
    // };
}

NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc) {
}

NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData var) {
}

NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc) {
}

// NkIrRef nkir_makeExtSymRef(NkIrProg ir, NkIrExtSymId sym) {
//     return {
//         .index = sym.id,
//         .offset = 0,
//         .type = ir->exsyms[sym.id].type,
//         .kind = NkIrRef_ExtSym,
//         .is_indirect = false,
//     };
// }

NkIrInstr nkir_make_nop() {
    return {{}, nkir_nop};
}

NkIrInstr nkir_make_ret() {
    return {{}, nkir_ret};
}

NkIrInstr nkir_make_jmp(NkIrLabel label) {
    return {{{}, _arg(label)}, nkir_jmp};
}

NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrLabel label) {
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpz};
}

NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrLabel label) {
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpnz};
}

NkIrInstr nkir_make_ext(NkIrRef dst, NkIrRef src) {
    return {{_arg(dst), _arg(src)}, nkir_ext};
}

NkIrInstr nkir_make_trunc(NkIrRef dst, NkIrRef src) {
    return {{_arg(dst), _arg(src)}, nkir_trunc};
}

NkIrInstr nkir_make_fp2i(NkIrRef dst, NkIrRef src) {
    return {{_arg(dst), _arg(src)}, nkir_fp2i};
}

NkIrInstr nkir_make_i2fp(NkIrRef dst, NkIrRef src) {
    return {{_arg(dst), _arg(src)}, nkir_i2fp};
}

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    return {{_arg(dst), _arg(proc), _arg(args)}, nkir_call};
}

NkIrInstr nkir_make_mov(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_lea(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_neg(NkIrRef dst, NkIrRef arg) {
}

NkIrInstr nkir_make_add(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_sub(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_mul(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_div(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_mod(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_and(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_or(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_xor(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_lsh(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_rsh(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_cmp_eq(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_cmp_ne(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_cmp_lt(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_cmp_le(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_cmp_gt(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_cmp_ge(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) {
}

NkIrInstr nkir_make_label(NkIrLabel label) {
}

void nkir_write(NkIrProg ir, NkbOutputKind kind, nkstr filepath) {
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir) {
}

void nkir_freeRunCtx(NkIrRunCtx ctx) {
}

void nkir_invoke(NkIrProc proc, NkIrPtrArray args, NkIrPtrArray ret) {
    // auto proc = nkval_as(NkIrProc, proc_val);

    // if (!proc->ir->bc) {
    //     proc->ir->bc = nkbc_create(proc->ir);
    // }

    // nkbc_invoke(proc->ir->bc, proc, ret, args);
}
