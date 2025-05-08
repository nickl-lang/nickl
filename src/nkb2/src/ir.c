#include "nkb/ir.h"

#include "ntk/common.h"
#include "ntk/log.h"

NK_LOG_USE_SCOPE(ir);

static NkIrArg argNull() {
    return (NkIrArg){0};
}

static NkIrArg argRef(NkIrRef ref) {
    return (NkIrArg){
        .ref = ref,
        .kind = NkIrArg_Ref,
    };
}

static NkIrArg argRefArray(NkIrRefArray refs) {
    return (NkIrArg){
        .refs = refs,
        .kind = NkIrArg_RefArray,
    };
}

static NkIrArg argLabel(NkAtom label) {
    return (NkIrArg){
        .label = {label, 0},
        .kind = NkIrArg_Label,
    };
}

static NkIrArg argType(NkIrType type) {
    return (NkIrArg){
        .type = type,
        .kind = NkIrArg_Type,
    };
}

static NkIrArg argString(NkString str) {
    return (NkIrArg){
        .str = str,
        .kind = NkIrArg_String,
    };
}

static NkIrArg argIdx(u32 idx) {
    return (NkIrArg){
        .idx = idx,
        .kind = NkIrArg_Idx,
    };
}

void nkir_convertToPic(NkIrInstrArray instrs, NkIrInstrDynArray *out) {
    NK_LOG_TRC("%s", __func__);

    (void)instrs;
    (void)out;
    nk_assert(!"TODO: `nkir_convertToPic` not implemented");
}

NkIrRef nkir_makeRefLocal(NkAtom sym, NkIrType type) {
    return (NkIrRef){
        .sym = sym,
        .type = type,
        .kind = NkIrRef_Local,
    };
}

NkIrRef nkir_makeRefParam(NkAtom sym, NkIrType type) {
    return (NkIrRef){
        .sym = sym,
        .type = type,
        .kind = NkIrRef_Param,
    };
}

NkIrRef nkir_makeRefGlobal(NkAtom sym, NkIrType type) {
    return (NkIrRef){
        .sym = sym,
        .type = type,
        .kind = NkIrRef_Global,
    };
}

NkIrRef nkir_makeRefImm(NkIrImm imm, NkIrType type) {
    return (NkIrRef){
        .imm = imm,
        .type = type,
        .kind = NkIrRef_Imm,
    };
}

NkIrInstr nkir_make_nop() {
    return (NkIrInstr){
        .arg = {argNull(), argNull(), argNull()},
        .code = NkIrOp_nop,
    };
}

NkIrInstr nkir_make_ret(NkIrRef arg) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(arg), argNull()},
        .code = NkIrOp_ret,
    };
}

NkIrInstr nkir_make_jmp(NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argNull(), argLabel(label)},
        .code = NkIrOp_jmp,
    };
}

NkIrInstr nkir_make_jmpz(NkIrRef cond, NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(cond), argLabel(label)},
        .code = NkIrOp_jmpz,
    };
}

NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(cond), argLabel(label)},
        .code = NkIrOp_jmpnz,
    };
}

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    return (NkIrInstr){
        .arg = {argRef(dst), argRef(proc), argRefArray(args)},
        .code = NkIrOp_call,
    };
}

NkIrInstr nkir_make_store(NkIrRef ptr, NkIrRef src) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(ptr), argRef(src)},
        .code = NkIrOp_store,
    };
}

NkIrInstr nkir_make_load(NkIrRef dst, NkIrRef ptr) {
    return (NkIrInstr){
        .arg = {argRef(dst), argRef(ptr), argNull()},
        .code = NkIrOp_load,
    };
}

NkIrInstr nkir_make_alloc(NkIrRef ptr, NkIrType type) {
    return (NkIrInstr){
        .arg = {argRef(ptr), argType(type), argNull()},
        .code = NkIrOp_alloc,
    };
}

#define UNA_IR(NAME)                                               \
    NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef arg) { \
        return (NkIrInstr){                                        \
            .arg = {argRef(dst), argRef(arg), argNull()},          \
            .code = NK_CAT(NkIrOp_, NAME),                         \
        };                                                         \
    }
#define BIN_IR(NAME)                                                            \
    NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        return (NkIrInstr){                                                     \
            .arg = {argRef(dst), argRef(lhs), argRef(rhs)},                     \
            .code = NK_CAT(NkIrOp_, NAME),                                      \
        };                                                                      \
    }
#define DBL_IR(NAME1, NAME2)                                                                               \
    NkIrInstr NK_CAT(nkir_make_, NK_CAT(NAME1, NK_CAT(_, NAME2)))(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        return (NkIrInstr){                                                                                \
            .arg = {argRef(dst), argRef(lhs), argRef(rhs)},                                                \
            .code = NK_CAT(NkIrOp_, NK_CAT(NAME1, NK_CAT(_, NAME2))),                                      \
        };                                                                                                 \
    }
#include "nkb/ir.inl"

NkIrInstr nkir_make_label(NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argNull(), argLabel(label)},
        .code = NkIrOp_label,
    };
}

NkIrInstr nkir_make_file(NkString file) {
    return (NkIrInstr){
        .arg = {argNull(), argString(file), argNull()},
        .code = NkIrOp_file,
    };
}

NkIrInstr nkir_make_line(u32 line) {
    return (NkIrInstr){
        .arg = {argNull(), argIdx(line), argNull()},
        .code = NkIrOp_line,
    };
}

NkIrInstr nkir_make_comment(NkString comment) {
    return (NkIrInstr){
        .arg = {argNull(), argString(comment), argNull()},
        .code = NkIrOp_comment,
    };
}

void nkir_exportModule(NkIrModule m, NkString path /*, c_compiler_config */) {
    NK_LOG_TRC("%s", __func__);

    (void)m;
    (void)path;
    nk_assert(!"TODO: `nkir_exportModule` not implemented");
}

NkIrRunCtx nkir_createRunCtx(NkIrModule *m, NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    (void)m;
    (void)arena;
    nk_assert(!"TODO: `nkir_createRunCtx` not implemented");
}

bool nkir_invoke(NkIrRunCtx ctx, NkAtom sym, void **args, void **ret) {
    NK_LOG_TRC("%s", __func__);

    (void)ctx;
    (void)sym;
    (void)args;
    (void)ret;
    nk_assert(!"TODO: `nkir_invoke` not implemented");
}

void nkir_inspectModule(NkIrModule m, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    for (usize i = 0; i < m.size; i++) {
        NkIrSymbol const *sym = &m.data[i];
        nkir_inspectSymbol(sym, out);
    }
}

void nkir_inspectSymbol(NkIrSymbol const *sym, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    switch (sym->kind) {
        case NkIrSymbol_Extern:
            break;

        case NkIrSymbol_Data:
            break;

        case NkIrSymbol_Proc:
            break;
    }
}

void nkir_inspectInstr(NkIrInstr instr, NkStream out) {
    NK_LOG_TRC("%s", __func__);
}

void nkir_inspectRef(NkIrRef ref, NkStream out) {
    NK_LOG_TRC("%s", __func__);
}

bool nkir_validateModule(NkIrModule m) {
    NK_LOG_TRC("%s", __func__);

    (void)m;
    nk_assert(!"TODO: `nkir_validateModule` not implemented");
}

bool nkir_validateProc(NkIrProc const *proc) {
    NK_LOG_TRC("%s", __func__);

    (void)proc;
    nk_assert(!"TODO: `nkir_validateProc` not implemented");
}
