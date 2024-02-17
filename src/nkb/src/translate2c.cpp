#include "translate2c.h"

#include <cmath>
#include <limits>

#include "ir_impl.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_map.hpp"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(translate2c);

struct DataFp {
    usize idx;
    void *data;
    usize type_id;
};

struct DataFpHashSetContext {
    static u64 hash(DataFp const &val) {
        u64 seed = 0;
        nk_hashCombine(&seed, val.idx);
        nk_hashCombine(&seed, (usize)val.data);
        nk_hashCombine(&seed, val.type_id);
        return seed;
    }

    static bool equal_to(DataFp const &lhs, DataFp const &rhs) {
        return lhs.idx == rhs.idx && lhs.data == rhs.data && lhs.type_id == rhs.type_id;
    }
};

typedef NkDynArray(bool) FlagArray;

bool &getFlag(FlagArray &ar, usize i) {
    while (i >= ar.size) {
        nkda_append(&ar, false);
    }
    return ar.data[i];
}

struct WriterCtx {
    NkIrProg ir;

    NkArena *arena;
    NkAllocator alloc = nk_arena_getAllocator(arena);

    NkStringBuilder types_s{0, 0, 0, alloc};
    NkStringBuilder forward_s{0, 0, 0, alloc};
    NkStringBuilder main_s{0, 0, 0, alloc};

    NkHashMap<usize, NkString> type_map = decltype(type_map)::create(alloc);
    usize typedecl_count = 0;

    NkHashMap<DataFp, NkString, DataFpHashSetContext> data_map = decltype(data_map)::create(alloc);
    usize data_count = 0;

    FlagArray procs_translated{0, 0, 0, alloc};
    FlagArray data_translated{0, 0, 0, alloc};
    FlagArray ext_data_translated{0, 0, 0, alloc};
    FlagArray ext_procs_translated{0, 0, 0, alloc};

    NkDynArray(usize) procs_to_translate{0, 0, 0, alloc};
};

#define LOCAL_CLASS "var"
#define ARG_CLASS "arg"
#define CONST_CLASS "const"
#define GLOBAL_CLASS "global"

void writeName(NkAtom name, usize index, char const *obj_class, NkStringBuilder *src) {
    if (name != NK_ATOM_INVALID) {
        auto const str = nk_atom2s(name);
        nksb_printf(src, NKS_FMT, NKS_ARG(str));
    } else {
        nksb_printf(src, "_%s_%zu", obj_class, index);
    }
}

void writePreamble(NkStringBuilder *sb) {
    nksb_printf(sb, R"(
typedef signed char i8;
typedef signed short i16;
typedef signed long i32;
typedef signed long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

)");
}

void writeVisibilityAttr(NkIrVisibility vis, NkStringBuilder *src) {
    switch (vis) {
    case NkIrVisibility_Default:
        nksb_printf(src, "__attribute__((visibility(\"default\"))) ");
        break;
    case NkIrVisibility_Hidden:
        break;
    case NkIrVisibility_Protected:
        nksb_printf(src, "__attribute__((visibility(\"protected\"))) ");
        break;
    case NkIrVisibility_Internal:
        nksb_printf(src, "__attribute__((visibility(\"internal\"))) ");
        break;
    case NkIrVisibility_Local:
        nksb_printf(src, "static ");
        break;
    }
}

void writeNumericType(NkIrNumericValueType value_type, NkStringBuilder *src) {
    switch (value_type) {
    case Int8:
        nksb_printf(src, "i8");
        break;
    case Int16:
        nksb_printf(src, "i16");
        break;
    case Int32:
        nksb_printf(src, "i32");
        break;
    case Int64:
        nksb_printf(src, "i64");
        break;
    case Uint8:
        nksb_printf(src, "u8");
        break;
    case Uint16:
        nksb_printf(src, "u16");
        break;
    case Uint32:
        nksb_printf(src, "u32");
        break;
    case Uint64:
        nksb_printf(src, "u64");
        break;
    case Float32:
        nksb_printf(src, "f32");
        break;
    case Float64:
        nksb_printf(src, "f64");
        break;
    default:
        nk_assert(!"unreachable");
        break;
    }
}

void writeType(WriterCtx &ctx, nktype_t type, NkStringBuilder *src, bool allow_void = false) {
    if (type->size == 0 && allow_void) {
        nksb_printf(src, "void");
        return;
    }

    auto found_str = ctx.type_map.find(type->id);
    if (found_str) {
        nksb_printf(src, NKS_FMT, NKS_ARG(*found_str));
        return;
    }

    NkStringBuilder tmp_s{0, 0, 0, ctx.alloc};
    NkStringBuilder tmp_s_suf{0, 0, 0, ctx.alloc};
    bool is_complex = false;

    switch (type->kind) {
    case NkIrType_Aggregate:
        is_complex = true;
        nksb_printf(&tmp_s, "struct {\n");
        for (usize i = 0; i < type->as.aggr.elems.size; i++) {
            nksb_printf(&tmp_s, "  ");
            writeType(ctx, type->as.aggr.elems.data[i].type, &tmp_s);
            nksb_printf(&tmp_s, " _%zu", i);
            if (type->as.aggr.elems.data[i].count > 1) {
                nksb_printf(&tmp_s, "[%zu]", type->as.aggr.elems.data[i].count);
            }
            nksb_printf(&tmp_s, ";\n");
        }
        nksb_printf(&tmp_s, "}");
        break;
    case NkIrType_Numeric:
        writeNumericType(type->as.num.value_type, &tmp_s);
        break;
    case NkIrType_Pointer:
        writeType(ctx, type->as.ptr.target_type, &tmp_s);
        nksb_printf(&tmp_s, "*");
        break;
    case NkIrType_Procedure: {
        is_complex = true;
        auto const ret_t = type->as.proc.info.ret_t;
        auto const args_t = type->as.proc.info.args_t;
        auto const is_variadic = type->as.proc.info.flags & NkProcVariadic;
        writeType(ctx, ret_t, &tmp_s, true);
        nksb_printf(&tmp_s, " (*");
        nksb_printf(&tmp_s_suf, ")(");
        for (usize i = 0; i < args_t.size; i++) {
            if (i) {
                nksb_printf(&tmp_s_suf, ", ");
            }
            writeType(ctx, args_t.data[i], &tmp_s_suf);
        }
        if (is_variadic) {
            nksb_printf(&tmp_s_suf, ", ...");
        }
        nksb_printf(&tmp_s_suf, ")");
        break;
    }
    default:
        nk_assert(!"unreachable");
        break;
    }

    NkString type_str{NK_SLICE_INIT(tmp_s)};

    if (is_complex) {
        nksb_printf(
            &ctx.types_s,
            "typedef " NKS_FMT " _type%zu" NKS_FMT ";\n",
            NKS_ARG(type_str),
            ctx.typedecl_count,
            NKS_ARG(tmp_s_suf));

        NkStringBuilder sb{0, 0, 0, ctx.alloc};
        nksb_printf(&sb, "_type%zu", ctx.typedecl_count);
        type_str = NkString{NK_SLICE_INIT(sb)};

        ctx.typedecl_count++;
    }

    ctx.type_map.insert(type->id, type_str);
    nksb_printf(src, NKS_FMT, NKS_ARG(type_str));
}

void writeData(WriterCtx &ctx, usize idx, NkIrDecl_T const &decl, NkStringBuilder *src, bool is_complex = false) {
    DataFp data_fp{idx, decl.data, decl.type->id};

    auto found_str = ctx.data_map.find(data_fp);
    if (found_str) {
        nksb_printf(src, NKS_FMT, NKS_ARG(*found_str));
        return;
    }

    is_complex |= decl.visibility != NkIrVisibility_Local;

    NkStringBuilder tmp_s{0, 0, 0, ctx.alloc};

    if (decl.data) {
        switch (decl.type->kind) {
        case NkIrType_Aggregate: {
            is_complex = true;
            nksb_printf(&tmp_s, "{ ");
            for (usize i = 0; i < decl.type->as.aggr.elems.size; i++) {
                auto const &elem = decl.type->as.aggr.elems.data[i];
                if (elem.count > 1) {
                    nksb_printf(&tmp_s, "{ ");
                }
                usize offset = elem.offset;
                for (usize c = 0; c < elem.count; c++) {
                    writeData(
                        ctx,
                        NKIR_INVALID_IDX,
                        {
                            .name = NK_ATOM_INVALID,
                            .data = (u8 *)decl.data + offset,
                            .type = elem.type,
                            .visibility = NkIrVisibility_Local,
                            .read_only = decl.read_only,
                        },
                        &tmp_s);
                    nksb_printf(&tmp_s, ", ");
                    offset += elem.type->size;
                }
                if (elem.count > 1) {
                    nksb_printf(&tmp_s, "}, ");
                }
            }
            nksb_printf(&tmp_s, "}");
            break;
        }
        case NkIrType_Numeric: {
            auto value_type = decl.type->as.num.value_type;
            switch (value_type) {
            case Int8:
                nksb_printf(&tmp_s, "%" PRIi8, *(i8 *)decl.data);
                break;
            case Uint8:
                nksb_printf(&tmp_s, "%" PRIu8, *(u8 *)decl.data);
                break;
            case Int16:
                nksb_printf(&tmp_s, "%" PRIi16, *(i16 *)decl.data);
                break;
            case Uint16:
                nksb_printf(&tmp_s, "%" PRIu16, *(u16 *)decl.data);
                break;
            case Int32:
                nksb_printf(&tmp_s, "%" PRIi32, *(i32 *)decl.data);
                break;
            case Uint32:
                nksb_printf(&tmp_s, "%" PRIu32, *(u32 *)decl.data);
                break;
            case Int64:
                nksb_printf(&tmp_s, "%" PRIi64, *(i64 *)decl.data);
                break;
            case Uint64:
                nksb_printf(&tmp_s, "%" PRIu64, *(u64 *)decl.data);
                break;
            case Float32: {
                auto f_val = *(f32 *)decl.data;
                nksb_printf(&tmp_s, "%.*g", std::numeric_limits<f32>::max_digits10, f_val);
                if (f_val == round(f_val)) {
                    nksb_printf(&tmp_s, ".");
                }
                nksb_printf(&tmp_s, "f");
                break;
            }
            case Float64: {
                auto f_val = *(f64 *)decl.data;
                nksb_printf(&tmp_s, "%.*lg", std::numeric_limits<f64>::max_digits10, f_val);
                if (f_val == round(f_val)) {
                    nksb_printf(&tmp_s, ".");
                }
                break;
            }
            default:
                nk_assert(!"unreachable");
                break;
            }
            if (value_type < Float32) {
                if (value_type == Uint8 || value_type == Uint16 || value_type == Uint32 || value_type == Uint64) {
                    nksb_printf(&tmp_s, "u");
                }
                if (decl.type->size == 4) {
                    nksb_printf(&tmp_s, "l");
                } else if (decl.type->size == 8) {
                    nksb_printf(&tmp_s, "ll");
                }
            }
            break;
        }
        default:
            nk_assert(!"unreachable");
            break;
        }
    }

    NkString str{NK_SLICE_INIT(tmp_s)};

    if (is_complex && !getFlag(ctx.data_translated, idx)) {
        writeVisibilityAttr(decl.visibility, &ctx.forward_s);
        writeType(ctx, decl.type, &ctx.forward_s);
        if (decl.read_only) {
            nksb_printf(&ctx.forward_s, " const");
        }
        nksb_printf(&ctx.forward_s, " ");
        writeName(decl.name, ctx.data_count, CONST_CLASS, &ctx.forward_s);
        nksb_printf(&ctx.forward_s, " = ");
        if (decl.data) {
            nksb_printf(&ctx.forward_s, NKS_FMT, NKS_ARG(str));
        } else {
            nksb_printf(&ctx.forward_s, "{0}");
        }
        nksb_printf(&ctx.forward_s, ";\n");

        NkStringBuilder sb{0, 0, 0, ctx.alloc};
        writeName(decl.name, ctx.data_count, CONST_CLASS, &sb);
        str = {NK_SLICE_INIT(sb)};

        ctx.data_count++;

        if (idx != NKIR_INVALID_IDX) {
            getFlag(ctx.data_translated, idx) = true;
        }
    }

    ctx.data_map.insert(data_fp, str);
    nksb_printf(src, NKS_FMT, NKS_ARG(str));
}

void writeProcSignature(
    WriterCtx &ctx,
    NkStringBuilder *src,
    NkString name,
    nktype_t ret_t,
    NkTypeArray args_t,
    NkAtomArray arg_names,
    bool va = false) {
    writeType(ctx, ret_t, src, true);
    nksb_printf(src, " " NKS_FMT "(", NKS_ARG(name));

    for (usize i = 0; i < args_t.size; i++) {
        if (i) {
            nksb_printf(src, ", ");
        }
        writeType(ctx, args_t.data[i], src);
        if (arg_names.size) {
            nksb_printf(src, " ");
            writeName(i < arg_names.size ? arg_names.data[i] : (NkAtom)NK_ATOM_INVALID, i, ARG_CLASS, src);
        }
    }
    if (va) {
        nksb_printf(src, ", ...");
    }
    nksb_printf(src, ")");
}

void writeCast(WriterCtx &ctx, NkStringBuilder *src, nktype_t type) {
    if (type->kind != NkIrType_Numeric && type->kind != NkIrType_Pointer && type->size) {
        return;
    }

    nksb_printf(src, "(");
    writeType(ctx, type, src);
    nksb_printf(src, ")");
}

void writeLineDirective(NkAtom file, usize line, NkStringBuilder *src) {
    if (file != NK_ATOM_INVALID) {
        auto const file_name = nk_atom2s(file);
        nksb_printf(src, "#line %zu \"", line);
        nks_escape(nksb_getStream(src), file_name);
        nksb_printf(src, "\"\n");
    } else {
        nksb_printf(src, "#line %zu\n", line);
    }
}

void writeLabel(WriterCtx &ctx, usize label_id, NkStringBuilder *src) {
    auto const &block = ctx.ir->blocks.data[label_id];
    auto name = nk_atom2s(block.name);
    if (name.data[0] == '@') {
        name.data++;
        name.size--;
    }
    nksb_printf(src, "l_" NKS_FMT, NKS_ARG(name));
}

void translateProc(WriterCtx &ctx, usize proc_id) {
    NK_PROF_FUNC();

    if (getFlag(ctx.procs_translated, proc_id)) {
        return;
    }

    NK_LOG_TRC("%s", __func__);
    NK_LOG_DBG("proc_id=%zu", proc_id);

    getFlag(ctx.procs_translated, proc_id) = true;

    auto const &proc = ctx.ir->procs.data[proc_id];

    auto proc_t = proc.proc_t;
    auto args_t = proc_t->as.proc.info.args_t;
    auto ret_t = proc_t->as.proc.info.ret_t;

    auto src = &ctx.main_s;

    writeVisibilityAttr(proc.visibility, &ctx.forward_s);
    writeProcSignature(ctx, &ctx.forward_s, nk_atom2s(proc.name), ret_t, args_t, {});
    nksb_printf(&ctx.forward_s, ";\n");

    nksb_printf(src, "\n");
    writeLineDirective(proc.file, proc.start_line, src);
    writeProcSignature(ctx, src, nk_atom2s(proc.name), ret_t, args_t, proc.arg_names);
    nksb_printf(src, " {\n\n");

    for (usize i = 0; auto decl : nk_iterate(proc.locals)) {
        writeLineDirective(NK_ATOM_INVALID, proc.start_line, src);
        writeType(ctx, decl.type, src);
        nksb_printf(src, " ");
        writeName(decl.name, i++, LOCAL_CLASS, src);
        nksb_printf(src, "={");
        if (decl.type->size) {
            nksb_printf(src, "0");
        }
        nksb_printf(src, "};\n");
    }

    if (ret_t->size) {
        writeLineDirective(NK_ATOM_INVALID, proc.start_line, src);
        writeType(ctx, ret_t, src);
        nksb_printf(src, " _ret={0};\n");
    }

    nksb_printf(src, "\n");

    auto write_ref = [&](NkIrRef const &ref) {
        NK_PROF_SCOPE(nk_cs2s("write_ref"));
        if (ref.kind == NkIrRef_Proc) {
            auto const &proc = ctx.ir->procs.data[ref.index];
            auto const proc_name = nk_atom2s(proc.name);
            nksb_printf(src, NKS_FMT, NKS_ARG(proc_name));
            if (!getFlag(ctx.procs_translated, ref.index)) {
                nkda_append(&ctx.procs_to_translate, ref.index);
            }
            return;
        } else if (ref.kind == NkIrRef_ExternProc) {
            auto const extern_proc = ctx.ir->extern_procs.data[ref.index];
            auto const extern_proc_name = nk_atom2s(extern_proc.name);
            nksb_printf(src, NKS_FMT, NKS_ARG(extern_proc_name));
            if (!getFlag(ctx.ext_procs_translated, ref.index)) {
                nksb_printf(&ctx.forward_s, "extern ");
                writeProcSignature(
                    ctx,
                    &ctx.forward_s,
                    extern_proc_name,
                    extern_proc.type->as.proc.info.ret_t,
                    extern_proc.type->as.proc.info.args_t,
                    {},
                    extern_proc.type->as.proc.info.flags & NkProcVariadic);
                nksb_printf(&ctx.forward_s, ";\n");
                getFlag(ctx.ext_procs_translated, ref.index) = true;
            }
            return;
        } else if (ref.kind == NkIrRef_ExternData) {
            auto const extern_data = ctx.ir->extern_data.data[ref.index];
            auto const extern_data_name = nk_atom2s(extern_data.name);
            nksb_printf(src, NKS_FMT, NKS_ARG(extern_data_name));
            if (!getFlag(ctx.ext_data_translated, ref.index)) {
                nksb_printf(&ctx.forward_s, "extern ");
                writeType(ctx, extern_data.type, &ctx.forward_s);
                nksb_printf(&ctx.forward_s, " " NKS_FMT ";\n", NKS_ARG(extern_data_name));
                getFlag(ctx.ext_data_translated, ref.index) = true;
            }
            return;
        } else if (ref.kind == NkIrRef_Address) {
            nksb_printf(src, "&");
            auto const &target_ref = ctx.ir->relocs.data[ref.index];
            writeData(ctx, target_ref.index, ctx.ir->data.data[target_ref.index], src, true);
            return;
        }

        bool const is_addressable =
            !(ref.kind == NkIrRef_Data && ctx.ir->data.data[ref.index].read_only &&
              ctx.ir->data.data[ref.index].type->kind != NkIrType_Aggregate);

        if (is_addressable) {
            for (u8 i = 0; i < nk_maxu(ref.indir, 1); i++) {
                nksb_printf(src, "*");
            }
            nksb_printf(src, "(");
            writeType(ctx, ref.type, src);
            for (u8 i = 0; i < nk_maxu(ref.indir, 1); i++) {
                nksb_printf(src, "*");
            }
            nksb_printf(src, ")");
            if (ref.post_offset) {
                nksb_printf(src, "((u8*)");
            }
            if (ref.offset) {
                nksb_printf(src, "((u8*)");
            }
            if (ref.indir == 0) {
                nksb_printf(src, "& ");
            }
        }
        switch (ref.kind) {
        case NkIrRef_Frame:
            writeName(proc.locals.data[ref.index].name, ref.index, LOCAL_CLASS, src);
            break;
        case NkIrRef_Arg:
            writeName(
                ref.index < proc.arg_names.size ? proc.arg_names.data[ref.index] : (NkAtom)NK_ATOM_INVALID,
                ref.index,
                ARG_CLASS,
                src);
            break;
        case NkIrRef_Ret:
            nksb_printf(src, "_ret");
            break;
        case NkIrRef_Data:
            writeData(ctx, ref.index, ctx.ir->data.data[ref.index], src);
            break;
        case NkIrRef_Proc:
        case NkIrRef_ExternProc:
        case NkIrRef_ExternData:
        case NkIrRef_Address:
        case NkIrRef_None:
        default:
            nk_assert(!"unreachable");
            break;
        }
        if (is_addressable) {
            if (ref.offset) {
                nksb_printf(src, "+%zu)", ref.offset);
            }
            if (ref.post_offset) {
                nksb_printf(src, "+%zu)", ref.post_offset);
            }
        }
    };

    for (auto bi : nk_iterate(proc.blocks)) {
        auto const &block = ctx.ir->blocks.data[bi];

        writeLabel(ctx, bi, src);
        nksb_printf(src, ":\n");

        usize last_line{};

        for (auto ii : nk_iterate(block.instrs)) {
            auto const &instr = ctx.ir->instrs.data[ii];

            if (instr.line != last_line + 1) {
                writeLineDirective(NK_ATOM_INVALID, instr.line, src);
            }
            last_line = instr.line;

            switch (instr.code) {
            case nkir_nop:
            case nkir_comment:
                continue;

            default:
                break;
            }

            nksb_printf(src, "  ");

            if (instr.arg[0].kind == NkIrArg_Ref && instr.arg[0].ref.kind != NkIrRef_None) {
                write_ref(instr.arg[0].ref);
                nksb_printf(src, " = ");
                writeCast(ctx, src, instr.arg[0].ref.type);
            }

            switch (instr.code) {
            case nkir_ret:
                nksb_printf(src, "return");
                if (ret_t->size) {
                    nksb_printf(src, " _ret");
                }
                break;
            case nkir_jmp: {
                nksb_printf(src, "goto ");
                writeLabel(ctx, instr.arg[1].id, src);
                break;
            }
            case nkir_jmpz: {
                nksb_printf(src, "if (0 == ");
                write_ref(instr.arg[1].ref);
                nksb_printf(src, ") { goto ");
                writeLabel(ctx, instr.arg[2].id, src);
                nksb_printf(src, "; }");
                break;
            }
            case nkir_jmpnz: {
                nksb_printf(src, "if (");
                write_ref(instr.arg[1].ref);
                nksb_printf(src, ") { goto ");
                writeLabel(ctx, instr.arg[2].id, src);
                nksb_printf(src, "; }");
                break;
            }
            case nkir_call: {
                auto proc_t = instr.arg[1].ref.type;
                nk_assert(proc_t->kind == NkIrType_Procedure);
                nksb_printf(src, "(");
                write_ref(instr.arg[1].ref);
                nksb_printf(src, ")(");
                for (usize i = 0; i < instr.arg[2].refs.size; i++) {
                    if (i) {
                        nksb_printf(src, ", ");
                    }
                    if (i < proc_t->as.proc.info.args_t.size) {
                        writeCast(ctx, src, proc_t->as.proc.info.args_t.data[i]);
                    }
                    write_ref(instr.arg[2].refs.data[i]);
                }
                nksb_printf(src, ")");
                break;
            }
            case nkir_mov:
                write_ref(instr.arg[1].ref);
                break;
            case nkir_lea:
                nksb_printf(src, "& ");
                write_ref(instr.arg[1].ref);
                break;

            case nkir_ext: {
                auto const dst_signed = NKIR_NUMERIC_IS_SIGNED(instr.arg[0].ref.type->as.num.value_type);
                switch (instr.arg[1].ref.type->as.num.value_type) {
                case Int8:
                case Uint8:
                    nksb_printf(src, dst_signed ? "(i8)" : "(u8)");
                    break;
                case Int16:
                case Uint16:
                    nksb_printf(src, dst_signed ? "(i16)" : "(u16)");
                    break;
                case Int32:
                case Uint32:
                    nksb_printf(src, dst_signed ? "(i32)" : "(u32)");
                    break;
                case Int64:
                case Uint64:
                    nksb_printf(src, dst_signed ? "(i64)" : "(u64)");
                    break;
                default:
                    break;
                }
                write_ref(instr.arg[1].ref);
                break;
            }

            case nkir_trunc:
            case nkir_fp2i:
            case nkir_i2fp:
                write_ref(instr.arg[1].ref);
                break;

#define UN_OP(NAME, OP)              \
    case NK_CAT(nkir_, NAME):        \
        nksb_printf(src, "(");       \
        nksb_printf(src, #OP);       \
        write_ref(instr.arg[1].ref); \
        nksb_printf(src, ")");       \
        break;

                UN_OP(neg, -)

#undef UN_OP

#define BIN_OP(NAME, OP)              \
    case NK_CAT(nkir_, NAME):         \
        nksb_printf(src, "(");        \
        write_ref(instr.arg[1].ref);  \
        nksb_printf(src, " " OP " "); \
        write_ref(instr.arg[2].ref);  \
        nksb_printf(src, ")");        \
        break;

                BIN_OP(add, "+")
                BIN_OP(sub, "-")
                BIN_OP(mul, "*")
                BIN_OP(div, "/")
                BIN_OP(mod, "%%")

                BIN_OP(and, "&")
                BIN_OP(or, "|")
                BIN_OP(xor, "^")
                BIN_OP(lsh, "<<")
                BIN_OP(rsh, ">>")

                BIN_OP(cmp_eq, "==")
                BIN_OP(cmp_ne, "!=")
                BIN_OP(cmp_lt, "<")
                BIN_OP(cmp_le, "<=")
                BIN_OP(cmp_gt, ">")
                BIN_OP(cmp_ge, ">=")

#undef BIN_OP

            case nkir_syscall: {
                nksb_printf(src, "syscall(");
                write_ref(instr.arg[1].ref);
                auto const args = instr.arg[2].refs;
                for (usize i = 0; i < args.size; i++) {
                    nksb_printf(src, ", ");
                    write_ref(args.data[i]);
                }
                nksb_printf(src, ")");
                break;
            }

            default:
                nk_assert(!"unreachable");
            }

            nksb_printf(src, ";\n");
        }

        nksb_printf(src, "\n");
    }

    writeLineDirective(NK_ATOM_INVALID, proc.end_line, src);
    nksb_printf(src, "}\n");
}

} // namespace

void nkir_translate2c(NkArena *arena, NkIrProg ir, NkStream src) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto frame = nk_arena_grab(arena);
    defer {
        nk_arena_popFrame(arena, frame);
    };

    WriterCtx ctx{
        .ir = ir,
        .arena = arena,
    };

    writePreamble(&ctx.types_s);

    for (usize i = 0; i < ir->procs.size; i++) {
        if (ir->procs.data[i].visibility != NkIrVisibility_Local) {
            translateProc(ctx, i);

            while (ctx.procs_to_translate.size) {
                auto proc = nk_slice_last(ctx.procs_to_translate);
                nkda_pop(&ctx.procs_to_translate, 1);
                translateProc(ctx, proc);
            }
        }
    }

    for (usize i = 0; i < ir->data.size; i++) {
        auto const &decl = ir->data.data[i];
        if (!getFlag(ctx.data_translated, i) && decl.visibility != NkIrVisibility_Local) {
            NkStringBuilder dummy_sb{0, 0, 0, ctx.alloc};
            writeData(ctx, i, decl, &dummy_sb, true);
        }
    }

#if 0
    fprintf(
        stderr, NKS_FMT "\n" NKS_FMT "\n" NKS_FMT, NKS_ARG(ctx.types_s), NKS_ARG(ctx.forward_s), NKS_ARG(ctx.main_s));
#endif

    nk_stream_printf(
        src, NKS_FMT "\n" NKS_FMT "\n" NKS_FMT, NKS_ARG(ctx.types_s), NKS_ARG(ctx.forward_s), NKS_ARG(ctx.main_s));
}
