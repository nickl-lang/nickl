#include "translate2c.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "ir_impl.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/hash_map.hpp"
#include "ntk/hash_set.hpp"
#include "ntk/id.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(translate2c);

struct ConstFp {
    void *data;
    size_t type_id;
};

struct ConstFpHashSetContext {
    static hash_t hash(ConstFp const &val) {
        hash_t seed = 0;
        hash_combine(&seed, (size_t)val.data);
        hash_combine(&seed, val.type_id);
        return seed;
    }

    static bool equal_to(ConstFp const &lhs, ConstFp const &rhs) {
        return lhs.data == rhs.data && lhs.type_id == rhs.type_id;
    }
};

struct WriterCtx {
    NkIrProg ir;

    NkArena *arena;
    NkAllocator alloc = nk_arena_getAllocator(arena);

    NkStringBuilder types_s{0, 0, 0, alloc};
    NkStringBuilder data_s{0, 0, 0, alloc};
    NkStringBuilder forward_s{0, 0, 0, alloc};
    NkStringBuilder main_s{0, 0, 0, alloc};

    NkHashMap<size_t, nks> type_map = decltype(type_map)::create(alloc);
    size_t typedecl_count = 0;

    NkHashMap<ConstFp, nks, ConstFpHashSetContext> const_map = decltype(const_map)::create(alloc);
    size_t const_count = 0;

    NkHashSet<size_t> procs_translated = decltype(procs_translated)::create(alloc);
    NkHashSet<size_t> procs_to_translate = decltype(procs_to_translate)::create(alloc);

    NkHashSet<size_t> ext_procs_forward_declared = decltype(ext_procs_forward_declared)::create(alloc);
    NkHashSet<size_t> globals_forward_declared = decltype(globals_forward_declared)::create(alloc);
};

void writeLocalName(NkIrProc_T const &proc, size_t id, NkStringBuilder *src) {
    auto const &decl = proc.locals.data[id];
    if (decl.name != nkid_empty) {
        auto const name = nkid2s(decl.name);
        nksb_printf(src, nks_Fmt, nks_Arg(name));
    } else {
        nksb_printf(src, "_var_%zu", id);
    }
}

void writeArgName(nkid_array arg_names, size_t id, NkStringBuilder *src) {
    if (id < arg_names.size && arg_names.data[id] != nkid_empty) {
        auto const name = nkid2s(arg_names.data[id]);
        nksb_printf(src, nks_Fmt, nks_Arg(name));
    } else {
        nksb_printf(src, "_arg_%zu", id);
    }
}

void writeGlobalName(NkIrProg ir, size_t id, NkStringBuilder *src) {
    auto const &decl = ir->globals.data[id];
    if (decl.name != nkid_empty) {
        auto const name = nkid2s(decl.name);
        nksb_printf(src, nks_Fmt, nks_Arg(name));
    } else {
        nksb_printf(src, "_global_%zu", id);
    }
}

void writePreamble(NkStringBuilder *sb) {
    nksb_printf(sb, R"(
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed long int32_t;
typedef signed long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

)");
}

void writeVisibilityAttr(NkIrVisibility vis, NkStringBuilder *src) {
    switch (vis) {
    case NkIrVisibility_Default:
        nksb_printf(src, "__attribute__((visibility(\"default\"))) ");
        break;
    case NkIrVisibility_Hidden:
        break;
    case NkIrVisibility_Local:
        nksb_printf(src, "static ");
        break;
    }
}

void writeNumericType(NkIrNumericValueType value_type, NkStringBuilder *src) {
    switch (value_type) {
    case Int8:
        nksb_printf(src, "int8_t");
        break;
    case Int16:
        nksb_printf(src, "int16_t");
        break;
    case Int32:
        nksb_printf(src, "int32_t");
        break;
    case Int64:
        nksb_printf(src, "int64_t");
        break;
    case Uint8:
        nksb_printf(src, "uint8_t");
        break;
    case Uint16:
        nksb_printf(src, "uint16_t");
        break;
    case Uint32:
        nksb_printf(src, "uint32_t");
        break;
    case Uint64:
        nksb_printf(src, "uint64_t");
        break;
    case Float32:
        nksb_printf(src, "float");
        break;
    case Float64:
        nksb_printf(src, "double");
        break;
    default:
        assert(!"unreachable");
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
        nksb_printf(src, nks_Fmt, nks_Arg(*found_str));
        return;
    }

    NkStringBuilder tmp_s{0, 0, 0, ctx.alloc};
    NkStringBuilder tmp_s_suf{0, 0, 0, ctx.alloc};
    bool is_complex = false;

    switch (type->kind) {
    case NkType_Aggregate:
        is_complex = true;
        nksb_printf(&tmp_s, "struct {\n");
        for (size_t i = 0; i < type->as.aggr.elems.size; i++) {
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
    case NkType_Numeric:
        writeNumericType(type->as.num.value_type, &tmp_s);
        break;
    case NkType_Pointer:
        writeType(ctx, type->as.ptr.target_type, &tmp_s);
        nksb_printf(&tmp_s, "*");
        break;
    case NkType_Procedure: {
        is_complex = true;
        auto const ret_t = type->as.proc.info.ret_t.data[0];
        auto const args_t = type->as.proc.info.args_t;
        auto const is_variadic = type->as.proc.info.flags & NkProcVariadic;
        writeType(ctx, ret_t, &tmp_s, true);
        nksb_printf(&tmp_s, " (*");
        nksb_printf(&tmp_s_suf, ")(");
        for (size_t i = 0; i < args_t.size; i++) {
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
        assert(!"unreachable");
        break;
    }

    nks type_str{nkav_init(tmp_s)};

    if (is_complex) {
        nksb_printf(
            &ctx.types_s,
            "typedef " nks_Fmt " type%zu" nks_Fmt ";\n",
            nks_Arg(type_str),
            ctx.typedecl_count,
            nks_Arg(tmp_s_suf));

        NkStringBuilder sb{0, 0, 0, ctx.alloc};
        nksb_printf(&sb, "type%zu", ctx.typedecl_count);
        type_str = nks{nkav_init(sb)};

        ctx.typedecl_count++;
    }

    ctx.type_map.insert(type->id, type_str);
    nksb_printf(src, nks_Fmt, nks_Arg(type_str));
}

void writeConst(WriterCtx &ctx, void *data, nktype_t type, NkStringBuilder *src, bool is_complex = false) {
    ConstFp const_fp{data, type->id};

    auto found_str = ctx.const_map.find(const_fp);
    if (found_str) {
        nksb_printf(src, nks_Fmt, nks_Arg(*found_str));
        return;
    }

    NkStringBuilder tmp_s{0, 0, 0, ctx.alloc};

    switch (type->kind) {
    case NkType_Aggregate: {
        is_complex = true;
        nksb_printf(&tmp_s, "{ ");
        for (size_t i = 0; i < type->as.aggr.elems.size; i++) {
            auto const &elem = type->as.aggr.elems.data[i];
            if (elem.count > 1) {
                nksb_printf(&tmp_s, "{ ");
            }
            size_t offset = elem.offset;
            for (size_t c = 0; c < elem.count; c++) {
                writeConst(ctx, (uint8_t *)data + offset, elem.type, &tmp_s);
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
    case NkType_Numeric: {
        auto value_type = type->as.num.value_type;
        switch (value_type) {
        case Int8:
            nksb_printf(&tmp_s, "%" PRIi8, *(int8_t *)data);
            break;
        case Uint8:
            nksb_printf(&tmp_s, "%" PRIu8, *(uint8_t *)data);
            break;
        case Int16:
            nksb_printf(&tmp_s, "%" PRIi16, *(int16_t *)data);
            break;
        case Uint16:
            nksb_printf(&tmp_s, "%" PRIu16, *(uint16_t *)data);
            break;
        case Int32:
            nksb_printf(&tmp_s, "%" PRIi32, *(int32_t *)data);
            break;
        case Uint32:
            nksb_printf(&tmp_s, "%" PRIu32, *(uint32_t *)data);
            break;
        case Int64:
            nksb_printf(&tmp_s, "%" PRIi64, *(int64_t *)data);
            break;
        case Uint64:
            nksb_printf(&tmp_s, "%" PRIu64, *(uint64_t *)data);
            break;
        case Float32: {
            auto f_val = *(float *)data;
            nksb_printf(&tmp_s, "%.*f", std::numeric_limits<float>::max_digits10, f_val);
            if (f_val == round(f_val)) {
                nksb_printf(&tmp_s, ".");
            }
            nksb_printf(&tmp_s, "f");
            break;
        }
        case Float64: {
            auto f_val = *(double *)data;
            nksb_printf(&tmp_s, "%.*lf", std::numeric_limits<double>::max_digits10, f_val);
            if (f_val == round(f_val)) {
                nksb_printf(&tmp_s, ".");
            }
            break;
        }
        default:
            assert(!"unreachable");
            break;
        }
        if (value_type < Float32) {
            if (value_type == Uint8 || value_type == Uint16 || value_type == Uint32 || value_type == Uint64) {
                nksb_printf(&tmp_s, "u");
            }
            if (type->size == 4) {
                nksb_printf(&tmp_s, "l");
            } else if (type->size == 8) {
                nksb_printf(&tmp_s, "ll");
            }
        }
        break;
    }
    default:
        assert(!"unreachable");
        break;
    }

    nks const_str{nkav_init(tmp_s)};

    if (is_complex) {
        nksb_printf(&ctx.data_s, "static ");
        writeType(ctx, type, &ctx.data_s);
        nksb_printf(&ctx.data_s, " _const%zu = " nks_Fmt ";\n", ctx.const_count, nks_Arg(const_str));

        NkStringBuilder sb{0, 0, 0, ctx.alloc};
        nksb_printf(&sb, "_const%zu", ctx.const_count);
        const_str = nks{nkav_init(sb)};

        ctx.const_count++;
    }

    ctx.const_map.insert(const_fp, const_str);
    nksb_printf(src, nks_Fmt, nks_Arg(const_str));
}

void writeProcSignature(
    WriterCtx &ctx,
    NkStringBuilder *src,
    nks name,
    nktype_t ret_t,
    NkTypeArray args_t,
    nkid_array arg_names,
    bool va = false) {
    writeType(ctx, ret_t, src, true);
    nksb_printf(src, " " nks_Fmt "(", nks_Arg(name));

    for (size_t i = 0; i < args_t.size; i++) {
        if (i) {
            nksb_printf(src, ", ");
        }
        writeType(ctx, args_t.data[i], src);
        if (arg_names.size) {
            nksb_printf(src, " ");
            writeArgName(arg_names, i, src);
        }
    }
    if (va) {
        nksb_printf(src, ", ...");
    }
    nksb_printf(src, ")");
}

void writeCast(WriterCtx &ctx, NkStringBuilder *src, nktype_t type) {
    if (type->kind != NkType_Numeric && type->kind != NkType_Pointer && type->size) {
        return;
    }

    nksb_printf(src, "(");
    writeType(ctx, type, src);
    nksb_printf(src, ")");
}

void writeGlobal(WriterCtx &ctx, size_t global_id, NkStringBuilder *src) {
    writeGlobalName(ctx.ir, global_id, src);
    if (!ctx.globals_forward_declared.find(global_id)) {
        auto const &decl = ctx.ir->globals.data[global_id];
        writeVisibilityAttr(decl.visibility, &ctx.forward_s);
        writeType(ctx, decl.type, &ctx.forward_s);
        nksb_printf(&ctx.forward_s, " ");
        writeGlobalName(ctx.ir, global_id, &ctx.forward_s);
        nksb_printf(&ctx.forward_s, "={");
        if (decl.type->size) {
            nksb_printf(&ctx.forward_s, "0");
        }
        nksb_printf(&ctx.forward_s, "};\n");
        ctx.globals_forward_declared.insert(global_id);
    }
}

void writeLineDirective(NkIrLine line, NkStringBuilder *src) {
    if (line.file != nkid_empty) {
        auto const file_name = nkid2s(line.file);
        nksb_printf(src, "#line %zu \"", line.line);
        nksb_str_escape(src, file_name);
        nksb_printf(src, "\"\n");
    }
}

void writeLabel(WriterCtx &ctx, size_t label_id, NkStringBuilder *src) {
    auto const &block = ctx.ir->blocks.data[label_id];
    auto name = nkid2s(block.name);
    if (name.data[0] == '@') {
        name.data++;
        name.size--;
    }
    nksb_printf(src, "l_" nks_Fmt, nks_Arg(name));
}

void translateProc(WriterCtx &ctx, NkIrProc proc_id) {
    if (ctx.procs_translated.find(proc_id.id)) {
        return;
    }

    NK_LOG_TRC("%s", __func__);
    NK_LOG_DBG("proc_id=%zu", proc_id.id);

    ctx.procs_translated.insert(proc_id.id);

    auto const &proc = ctx.ir->procs.data[proc_id.id];

    auto proc_t = proc.proc_t;
    auto args_t = proc_t->as.proc.info.args_t;
    auto ret_t = proc_t->as.proc.info.ret_t.data[0];

    auto src = &ctx.main_s;

    writeVisibilityAttr(proc.visibility, &ctx.forward_s);
    writeProcSignature(ctx, &ctx.forward_s, nkid2s(proc.name), ret_t, args_t, {});
    nksb_printf(&ctx.forward_s, ";\n");

    nksb_printf(src, "\n");
    writeLineDirective(proc.start_line, src);
    writeProcSignature(ctx, src, nkid2s(proc.name), ret_t, args_t, proc.arg_names);
    nksb_printf(src, " {\n\n");

    for (size_t i = 0; auto decl : nk_iterate(proc.locals)) {
        writeLineDirective(proc.start_line, src);
        writeType(ctx, decl.type, src);
        nksb_printf(src, " ");
        writeLocalName(proc, i++, src);
        nksb_printf(src, "={");
        if (decl.type->size) {
            nksb_printf(src, "0");
        }
        nksb_printf(src, "};\n");
    }

    if (ret_t->size) {
        writeLineDirective(proc.start_line, src);
        writeType(ctx, ret_t, src);
        nksb_printf(src, " ret={0};\n");
    }

    nksb_printf(src, "\n");

    auto write_ref = [&](NkIrRef const &ref) {
        if (ref.kind == NkIrRef_Proc) {
            auto const &proc = ctx.ir->procs.data[ref.index];
            auto const proc_name = nkid2s(proc.name);
            nksb_printf(src, nks_Fmt, nks_Arg(proc_name));
            if (!ctx.procs_translated.find(ref.index)) {
                ctx.procs_to_translate.insert(ref.index);
            }
            return;
        } else if (ref.kind == NkIrRef_Rodata) {
            auto const &cnst = ctx.ir->consts.data[ref.index];
            writeConst(ctx, cnst.data, cnst.type, src);
            return;
        } else if (ref.kind == NkIrRef_ExternProc) {
            auto const extern_proc = ctx.ir->extern_procs.data[ref.index];
            auto const extern_proc_name = nkid2s(extern_proc.name);
            nksb_printf(src, nks_Fmt, nks_Arg(extern_proc_name));
            if (!ctx.ext_procs_forward_declared.find(ref.index)) {
                nksb_printf(&ctx.forward_s, "extern ");
                writeProcSignature(
                    ctx,
                    &ctx.forward_s,
                    extern_proc_name,
                    extern_proc.type->as.proc.info.ret_t.data[0],
                    extern_proc.type->as.proc.info.args_t,
                    {},
                    extern_proc.type->as.proc.info.flags & NkProcVariadic);
                nksb_printf(&ctx.forward_s, ";\n");
                ctx.ext_procs_forward_declared.insert(ref.index);
            }
            return;
        } else if (ref.kind == NkIrRef_ExternData) {
            nksb_printf(src, "TODO NkIrRef_ExternData");
            return;
        } else if (ref.kind == NkIrRef_Address) {
            nksb_printf(src, "&");
            auto const &target_ref = ctx.ir->relocs.data[ref.index];
            switch (target_ref.kind) {
            case NkIrRef_Data:
                writeGlobal(ctx, target_ref.index, src);
                break;
            case NkIrRef_Rodata: {
                auto const &cnst = ctx.ir->consts.data[target_ref.index];
                writeConst(ctx, cnst.data, cnst.type, src, true);
                break;
            }
            default:
                assert(!"unreachable");
                break;
            }
            return;
        }

        uint8_t indir = ref.indir;
        if (ref.kind == NkIrRef_Arg || ref.kind == NkIrRef_Ret) {
            indir--;
        }
        for (uint8_t i = 0; i < maxu(indir, 1); i++) {
            nksb_printf(src, "*");
        }
        nksb_printf(src, "(");
        writeType(ctx, ref.type, src);
        for (uint8_t i = 0; i < maxu(indir, 1); i++) {
            nksb_printf(src, "*");
        }
        nksb_printf(src, ")");
        if (ref.post_offset) {
            nksb_printf(src, "((uint8_t*)");
        }
        if (ref.offset) {
            nksb_printf(src, "((uint8_t*)");
        }
        if (indir == 0) {
            nksb_printf(src, "& ");
        }
        switch (ref.kind) {
        case NkIrRef_Frame:
            writeLocalName(proc, ref.index, src);
            break;
        case NkIrRef_Arg:
            writeArgName(proc.arg_names, ref.index, src);
            break;
        case NkIrRef_Ret:
            nksb_printf(src, "ret");
            break;
        case NkIrRef_Data:
            writeGlobal(ctx, ref.index, src);
            break;
        case NkIrRef_Proc:
        case NkIrRef_Rodata:
        case NkIrRef_ExternProc:
        case NkIrRef_ExternData:
        case NkIrRef_Address:
        case NkIrRef_None:
        default:
            assert(!"unreachable");
            break;
        }
        if (ref.offset) {
            nksb_printf(src, "+%zu)", ref.offset);
        }
        if (ref.post_offset) {
            nksb_printf(src, "+%zu)", ref.post_offset);
        }
    };

    for (auto bi : nk_iterate(proc.blocks)) {
        auto const &block = ctx.ir->blocks.data[bi];

        writeLabel(ctx, bi, src);
        nksb_printf(src, ":\n");

        NkIrLine last_line{};

        for (auto ii : nk_iterate(block.instrs)) {
            auto const &instr = ctx.ir->instrs.data[ii];

            switch (instr.code) {
            case nkir_nop:
            case nkir_comment:
                continue;

            case nkir_line: {
                auto const cur_line = instr.arg[1].line;
                if (cur_line.file != last_line.file || cur_line.line != last_line.line + 1) {
                    writeLineDirective(cur_line, src);
                }
                last_line = cur_line;
                continue;
            }

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
                    nksb_printf(src, " ret");
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
                assert(proc_t->kind == NkType_Procedure);
                nksb_printf(src, "(");
                write_ref(instr.arg[1].ref);
                nksb_printf(src, ")(");
                for (size_t i = 0; i < instr.arg[2].refs.size; i++) {
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
                    nksb_printf(src, dst_signed ? "(int8_t)" : "(uint8_t)");
                    break;
                case Int16:
                case Uint16:
                    nksb_printf(src, dst_signed ? "(int16_t)" : "(uint16_t)");
                    break;
                case Int32:
                case Uint32:
                    nksb_printf(src, dst_signed ? "(int32_t)" : "(uint32_t)");
                    break;
                case Int64:
                case Uint64:
                    nksb_printf(src, dst_signed ? "(int64_t)" : "(uint64_t)");
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
    case CAT(nkir_, NAME):           \
        nksb_printf(src, "(");       \
        nksb_printf(src, #OP);       \
        write_ref(instr.arg[1].ref); \
        nksb_printf(src, ")");       \
        break;

                UN_OP(neg, -)

#undef UN_OP

#define BIN_OP(NAME, OP)              \
    case CAT(nkir_, NAME):            \
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

                // TODO Detect if target platform supports syscalls
            case nkir_syscall: {
                nksb_printf(src, "syscall(");
                write_ref(instr.arg[1].ref);
                auto const args = instr.arg[2].refs;
                for (size_t i = 0; i < args.size; i++) {
                    nksb_printf(src, ", ");
                    write_ref(args.data[i]);
                }
                nksb_printf(src, ")");
                break;
            }

            default:
                assert(!"unreachable");
            }

            nksb_printf(src, ";\n");
        }

        nksb_printf(src, "\n");
    }

    writeLineDirective(proc.end_line, src);
    nksb_printf(src, "}\n");
}

} // namespace

void nkir_translate2c(NkArena *arena, NkIrProg ir, nk_stream src) {
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

    for (size_t i = 0; i < ir->procs.size; i++) {
        if (ir->procs.data[i].visibility == NkIrVisibility_Default) {
            translateProc(ctx, {i});

            while (ctx.procs_to_translate.size()) {
                auto proc = *ctx.procs_to_translate.begin();
                ctx.procs_to_translate.remove(proc);
                translateProc(ctx, {proc});
            }
        }
    }

#if 0
    fprintf(
        stderr,
        nks_Fmt "\n" nks_Fmt "\n" nks_Fmt "\n" nks_Fmt,
        nks_Arg(ctx.types_s),
        nks_Arg(ctx.data_s),
        nks_Arg(ctx.forward_s),
        nks_Arg(ctx.main_s));
#endif

    nk_stream_printf(
        src,
        nks_Fmt "\n" nks_Fmt "\n" nks_Fmt "\n" nks_Fmt,
        nks_Arg(ctx.types_s),
        nks_Arg(ctx.data_s),
        nks_Arg(ctx.forward_s),
        nks_Arg(ctx.main_s));
}
