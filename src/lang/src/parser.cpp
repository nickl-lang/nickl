#include "parser.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"
#include "nkl/lang/ast.h"
#include "nkl/lang/token.h"
#include "token.hpp"

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_token_id[ID], s_token_text[ID]

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(AR.emplace_back(VAL))
#define EXPECT(ID) CHECK(expect(ID))

namespace {

NK_LOG_USE_SCOPE(parser);

struct ParseEngine {
    std::vector<NklToken> const &m_tokens;
    NklAst m_ast;

    bool m_error_occurred = false;

    NklTokenRef m_cur_token{};
    NklTokenRef m_last_token{};

    enum EExprKind {
        Expr_Regular,
        Expr_Bool,
        Expr_Type,
    };

    EExprKind m_cur_expr_kind = Expr_Regular;

    NklAstNode parse() {
        assert(m_tokens.size() && m_tokens.back().id == t_eof && "ill-formed token stream");

        m_cur_token = &m_tokens[0];
        // return nkl_ast_pushNode(m_ast, block(false));
        return nkl_ast_pushNode(m_ast, {});
    }

private:
#if 0
    struct Void {};

    void getToken() {
        assert(m_cur_token->id != t_eof);
        m_last_token = m_cur_token++;

        NK_LOG_DBG("next token: " LOG_TOKEN(m_cur_token->id));
    }

    bool accept(ETokenId id) {
        if (check(id)) {
            NK_LOG_DBG("accept" LOG_TOKEN(id));
            getToken();
            return true;
        }
        return false;
    }

    bool check(ETokenId id) const {
        return m_cur_token->id == id;
    }

    void expect(ETokenId id) {
        if (!accept(id)) {
            return error("'%s' expected", s_token_text[id]);
        }
    }

    Void sequence(
        std::vector<NklAstNode_T> &nodes,
        bool *allow_trailing_comma = nullptr,
        bool *all_ids = nullptr) {
        if (all_ids) {
            *all_ids = true;
        }
        do {
            if (check(t_par_r) && allow_trailing_comma) {
                *allow_trailing_comma = true;
                break;
            }
            APPEND(nodes, expr());
            if (all_ids && nodes.back().id != cs2nkid("id")) {
                *all_ids = false;
            }
        } while (accept(t_comma));
        return {};
    }

    struct ParseFieldsConfig {
        bool accept_const = true;
        bool accept_init = true;
        bool allow_tuple_mode = false;
    };

    struct ParseFieldsResult {
        std::vector<NklAstNode_T> fields;
        bool trailing_comma_parsed;
        bool variadic_parsed;
        bool tuple_parsed;
    };

    Void parseFields(ParseFieldsResult &res, ETokenId closer, ParseFieldsConfig const &cfg) {
        bool &tuple_mode = res.tuple_parsed = cfg.allow_tuple_mode;
        if (accept(closer)) {
            return {};
        }
        NklAstNode_T &field = res.fields.emplace_back();
        if (cfg.accept_const && accept(t_const)) {
            field.is_const = true;
            tuple_mode = false;
        }
        if (tuple_mode) {
            if (check(t_id)) {
                ASSIGN(field.name, identifier());
            }
            if (accept(t_colon)) {
                tuple_mode = false;
                ASSIGN(field.type, nkl_ast_pushNode(m_ast, expr(Expr_Type)));
            } else {
                if (field.name) {
                    m_cur_token--;
                }
                ASSIGN(field.type, nkl_ast_pushNode(m_ast, expr()));
            }
        } else {
            ASSIGN(field.name, identifier());
            EXPECT(t_colon);
            ASSIGN(field.type, nkl_ast_pushNode(m_ast, expr(Expr_Type)));
            if (cfg.accept_init && accept(t_eq)) {
                ASSIGN(field.init_value, nkl_ast_pushNode(m_ast, expr()));
            }
        }
        if (tuple_mode) {
            while (accept(t_comma)) {
                if (check(closer)) {
                    res.trailing_comma_parsed = true;
                    break;
                }
                NklAstNode_T &field = res.fields.emplace_back();
                ASSIGN(field.type, nkl_ast_pushNode(m_ast, expr()));
            }
        } else {
            while (accept(t_comma)) {
                if (accept(t_period_3x)) {
                    res.variadic_parsed = true;
                    break;
                }
                if (check(closer)) {
                    res.trailing_comma_parsed = true;
                    break;
                }
                NklAstNode_T &field = res.fields.emplace_back();
                if (cfg.accept_const && accept(t_const)) {
                    field.is_const = true;
                }
                ASSIGN(field.name, identifier());
                EXPECT(t_colon);
                ASSIGN(field.type, nkl_ast_pushNode(m_ast, expr(Expr_Type)));
                if (cfg.accept_init && accept(t_eq)) {
                    ASSIGN(field.init_value, nkl_ast_pushNode(m_ast, expr()));
                }
            }
        }
        EXPECT(closer);
        return {};
    }

    struct ParseArgsConfig {
        bool allow_trailing_comma;
    };

    Void parseArgs(std::vector<NklAstNode_T> &args, ETokenId closer, ParseArgsConfig const &cfg) {
        do {
            if (cfg.allow_trailing_comma && check(closer)) {
                break;
            }
            NklAstNode_T &arg = args.emplace_back();
            if (check(t_id)) {
                NK_LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
                auto id = m_cur_token;
                getToken();
                if (accept(t_eq)) {
                    arg.name = id;
                } else {
                    m_cur_token--;
                }
            }
            ASSIGN(arg.node, nkl_ast_pushNode(m_ast, expr()));
        } while (accept(t_comma));
        EXPECT(closer);
        return {};
    }

    NklAstNode_T block(bool capture_brace = true) {
        //@Refactor Optimize node allocation in parser for nodes
        std::vector<NklAstNode_T> nodes{};

        bool expect_brace_r = capture_brace && accept(t_brace_l);

        while (!check(t_eof) && (!expect_brace_r || !check(t_brace_r))) {
            NK_LOG_DBG("next statement: token=" LOG_TOKEN(m_cur_token->id));
            APPEND(nodes, statement());
            NK_LOG_DBG("end of statement");
        }

        if (expect_brace_r) {
            EXPECT(t_brace_r);
        }

        auto node = nodes.size() == 0
                        ? m_ast.make_nop()
                        : nodes.size() == 1 ? nodes.front() : m_ast.make_block(nodes.slice());

        return capture_brace ? m_ast.make_scope(node) : node;
    }

    NklTokenRef identifier() {
        if (!check(t_id)) {
            return error("identifier expected"), NklTokenRef{};
        }
        NK_LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
        auto id = m_cur_token;
        getToken();
        return id;
    }

    NklAstNode_T parseImport() {
        NklAstNode_T node{};
        if (check(t_string)) {
            NK_LOG_DBG("accept(string, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            ASSIGN(node, m_ast.make_import_path(m_cur_token)) getToken();
        } else {
            std::vector<NklTokenRef> tokens{};
            APPEND(tokens, identifier());
            while (accept(t_period)) {
                APPEND(tokens, identifier());
            }
            ASSIGN(node, m_ast.make_import(tokens));
        }
        return node;
    }

    NklAstNode_T statement() {
        NklAstNode_T node;

        if (accept(t_return)) {
            if (check(t_semi)) {
                node = m_ast.make_return({});
            } else {
                ASSIGN(node, m_ast.make_return(assignment()));
            }
        } else if (accept(t_break)) {
            node = m_ast.make_break();
        } else if (accept(t_continue)) {
            node = m_ast.make_continue();
        } else if (accept(t_if)) {
            DEFINE(cond, expr(Expr_Bool));
            DEFINE(then_body, statement());
            DEFINE(else_body, accept(t_else) ? statement() : NklAstNode_T{});
            node = m_ast.make_if(cond, then_body, else_body);
        } else if (accept(t_while)) {
            DEFINE(cond, expr(Expr_Bool));
            DEFINE(body, statement());
            node = m_ast.make_while(cond, body);
        } else if (check(t_brace_l)) {
            ASSIGN(node, block());
        } else if (accept(t_defer)) {
            ASSIGN(node, m_ast.make_defer_stmt(assignment()));
        } else if (accept(t_import)) {
            ASSIGN(node, parseImport());
        } else if (accept(t_for)) {
            DEFINE(it, identifier());
            bool const by_ptr = accept(t_period_aster);
            EXPECT(t_in);
            DEFINE(range, expr(Expr_Bool)); //@Todo Using bool for range in for
            DEFINE(body, statement());
            if (by_ptr) {
                ASSIGN(node, m_ast.make_for_by_ptr(it, range, body));
            } else {
                ASSIGN(node, m_ast.make_for(it, range, body));
            }
        } else if (check(t_tag)) {
            //@Todo Refactor token debug prints
            NK_LOG_DBG("accept(tag, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            auto tag = m_cur_token;
            getToken();
            if (accept(t_colon_2x)) {
                ASSIGN(node, m_ast.make_tag_def(tag, tuple()));
            } else {
                std::vector<NklAstNode_T> args{};
                if (check(t_par_l) || check(t_brace_l)) {
                    auto closer = check(t_par_l) ? t_par_r : t_brace_r;
                    getToken();
                    CHECK(parseArgs(
                        args,
                        closer,
                        {
                            .allow_trailing_comma = true,
                        }));
                }
                ASSIGN(node, m_ast.make_tag(tag, args, statement()));
            }
        } else if (accept(t_const)) {
            //@Boilerplate in const var decl
            DEFINE(name, identifier());
            EXPECT(t_colon);
            DEFINE(type, expr(Expr_Type));
            DEFINE(value, accept(t_eq) ? tuple() : NklAstNode_T{});
            ASSIGN(node, m_ast.make_const_decl(name, type, value));
        } else {
            ASSIGN(node, assignment());
        }

        //@Todo Hack for id-statements, also support multiple assignment
        if (node.id == cs2nkid("id")) {
            if (accept(t_colon_eq)) {
                ASSIGN(node, m_ast.make_define({&Node_token_value(&node), 1}, tuple()));
            } else if (accept(t_colon_2x)) {
                NklAstNode_T rhs{};
                if (accept(t_import)) {
                    ASSIGN(rhs, parseImport());
                } else {
                    ASSIGN(rhs, tuple());
                }
                ASSIGN(node, m_ast.make_comptime_const_def(Node_token_value(&node), rhs));
            } else if (accept(t_colon)) {
                DEFINE(type, expr(Expr_Type));
                DEFINE(value, accept(t_eq) ? tuple() : NklAstNode_T{});
                ASSIGN(node, m_ast.make_var_decl(Node_token_value(&node), type, value));
            }
        }

        if (m_last_token->id != t_semi && m_last_token->id != t_brace_r) {
            EXPECT(t_semi);
        }
        while (accept(t_semi)) {
        }

        return node;
    }

    NklAstNode_T assignment() {
        DEFINE(node, tuple());

        if (accept(t_eq)) {
            //@Todo Support multiple assignment in parser
            ASSIGN(node, m_ast.make_assign({nkl_ast_pushNode(m_ast, node), 1}, tuple()));
        } else if (accept(t_plus_eq)) {
            ASSIGN(node, m_ast.make_add_assign(node, tuple()));
        } else if (accept(t_minus_eq)) {
            ASSIGN(node, m_ast.make_sub_assign(node, tuple()));
        } else if (accept(t_aster_eq)) {
            ASSIGN(node, m_ast.make_mul_assign(node, tuple()));
        } else if (accept(t_slash_eq)) {
            ASSIGN(node, m_ast.make_div_assign(node, tuple()));
        } else if (accept(t_percent_eq)) {
            ASSIGN(node, m_ast.make_mod_assign(node, tuple()));
        } else if (accept(t_less_2x_eq)) {
            ASSIGN(node, m_ast.make_lsh_assign(node, tuple()));
        } else if (accept(t_greater_2x_eq)) {
            ASSIGN(node, m_ast.make_rsh_assign(node, tuple()));
        } else if (accept(t_bar_eq)) {
            ASSIGN(node, m_ast.make_bitor_assign(node, tuple()));
        } else if (accept(t_caret_eq)) {
            ASSIGN(node, m_ast.make_xor_assign(node, tuple()));
        } else if (accept(t_amper_eq)) {
            ASSIGN(node, m_ast.make_bitand_assign(node, tuple()));
        }

        return node;
    }

    NklAstNode_T tuple() {
        bool trailing_comma_provided = false;
        bool all_ids = false;
        std::vector<NklAstNode_T> nodes{};
        CHECK(sequence(nodes, &trailing_comma_provided, &all_ids));
        return (nodes.size() > 1 || trailing_comma_provided) ? m_ast.make_tuple(nodes)
                                                             : nodes.front();
    }

    NklAstNode_T expr(EExprKind kind = Expr_Regular) {
        auto const old_expr_kind = m_cur_expr_kind;
        m_cur_expr_kind = kind;
        defer {
            m_cur_expr_kind = old_expr_kind;
        };

        NklAstNode_T node;

        if (accept(t_struct)) {
            EXPECT(t_brace_l);
            ParseFieldsResult res{};
            CHECK(parseFields(
                res,
                t_brace_r,
                {
                    .accept_const = true,
                    .accept_init = true,
                    .allow_tuple_mode = false,
                }));
            ASSIGN(node, m_ast.make_struct(res.fields));
        } else if (accept(t_union)) {
            EXPECT(t_brace_l);
            ParseFieldsResult res{};
            CHECK(parseFields(
                res,
                t_brace_r,
                {
                    .accept_const = false,
                    .accept_init = false,
                    .allow_tuple_mode = false,
                }));
            ASSIGN(node, m_ast.make_union(res.fields));
        } else if (accept(t_enum)) {
            EXPECT(t_brace_l);
            ParseFieldsResult res{};
            CHECK(parseFields(
                res,
                t_brace_r,
                {
                    .accept_const = false,
                    .accept_init = false,
                    .allow_tuple_mode = false,
                }));
            ASSIGN(node, m_ast.make_enum(res.fields));
        } else {
            ASSIGN(node, ternary());
        }

        return node;
    }

    NklAstNode_T ternary() {
        DEFINE(node, logicOr());

        while (true) {
            if (accept(t_question)) {
                DEFINE(then_body, logicOr());
                EXPECT(t_colon);
                DEFINE(else_body, logicOr());
                node = m_ast.make_ternary(node, then_body, else_body);
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T logicOr() {
        DEFINE(node, logicAnd());

        while (true) {
            if (accept(t_bar_2x)) {
                ASSIGN(node, m_ast.make_or(node, logicOr()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T logicAnd() {
        DEFINE(node, bitOr());

        while (true) {
            if (accept(t_amper_2x)) {
                ASSIGN(node, m_ast.make_and(node, bitOr()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T bitOr() {
        DEFINE(node, bitXor());

        while (true) {
            if (accept(t_bar)) {
                ASSIGN(node, m_ast.make_bitor(node, bitXor()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T bitXor() {
        DEFINE(node, bitAnd());

        while (true) {
            if (accept(t_caret)) {
                ASSIGN(node, m_ast.make_xor(node, bitAnd()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T bitAnd() {
        DEFINE(node, equality());

        while (true) {
            if (accept(t_amper)) {
                ASSIGN(node, m_ast.make_bitand(node, equality()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T equality() {
        DEFINE(node, relation());

        while (true) {
            if (accept(t_eq_2x)) {
                ASSIGN(node, m_ast.make_eq(node, relation()));
            } else if (accept(t_exclam_eq)) {
                ASSIGN(node, m_ast.make_ne(node, relation()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T relation() {
        DEFINE(node, shift());

        while (true) {
            if (accept(t_less)) {
                ASSIGN(node, m_ast.make_lt(node, shift()));
            } else if (accept(t_less_eq)) {
                ASSIGN(node, m_ast.make_le(node, shift()));
            } else if (accept(t_greater)) {
                ASSIGN(node, m_ast.make_gt(node, shift()));
            } else if (accept(t_greater_eq)) {
                ASSIGN(node, m_ast.make_ge(node, shift()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T shift() {
        DEFINE(node, addSub());

        while (true) {
            if (accept(t_less_2x)) {
                ASSIGN(node, m_ast.make_lsh(node, addSub()));
            } else if (accept(t_greater_2x)) {
                ASSIGN(node, m_ast.make_rsh(node, addSub()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T addSub() {
        DEFINE(node, mulDiv());

        while (true) {
            if (accept(t_plus)) {
                ASSIGN(node, m_ast.make_add(node, mulDiv()));
            } else if (accept(t_minus)) {
                ASSIGN(node, m_ast.make_sub(node, mulDiv()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T mulDiv() {
        DEFINE(node, prefix());

        while (true) {
            if (accept(t_aster)) {
                ASSIGN(node, m_ast.make_mul(node, prefix()));
            } else if (accept(t_slash)) {
                ASSIGN(node, m_ast.make_div(node, prefix()));
            } else if (accept(t_percent)) {
                ASSIGN(node, m_ast.make_mod(node, prefix()));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T prefix() {
        NklAstNode_T node;

        if (accept(t_plus)) {
            ASSIGN(node, m_ast.make_uplus(suffix()));
        } else if (accept(t_minus)) {
            ASSIGN(node, m_ast.make_uminus(suffix()));
        } else if (accept(t_exclam)) {
            ASSIGN(node, m_ast.make_not(suffix()));
        } else if (accept(t_tilde)) {
            ASSIGN(node, m_ast.make_compl(suffix()));
        } else if (accept(t_amper)) {
            ASSIGN(node, m_ast.make_addr(prefix()));
        } else if (accept(t_aster)) {
            if (accept(t_const)) {
                ASSIGN(node, m_ast.make_const_ptr_type(prefix()));
            } else {
                ASSIGN(node, m_ast.make_ptr_type(prefix()));
            }
        } else if (accept(t_bracket_2x)) {
            ASSIGN(node, m_ast.make_slice_type(suffix()));
        } else if (accept(t_bracket_l)) {
            bool trailing_comma_provided = false;
            bool all_ids = false;
            std::vector<NklAstNode_T> nodes{};
            CHECK(sequence(nodes, &trailing_comma_provided, &all_ids));
            EXPECT(t_bracket_r);
            if (nodes.size() == 0) {
                return error("TODO ERR empty array"), NklAstNode_T{};
            }
            if (nodes.size() > 1 || check(t_par_r) || check(t_bracket_r) || check(t_bracket_r) ||
                check(t_comma) || check(t_semi)) {
                node = m_ast.make_array(nodes);
            } else {
                ASSIGN(node, m_ast.make_array_type(suffix(), nodes.front()));
            }
        } else if (accept(t_cast)) {
            EXPECT(t_par_l);
            DEFINE(lhs, expr(Expr_Type));
            EXPECT(t_par_r);
            DEFINE(rhs, prefix());
            ASSIGN(node, m_ast.make_cast(lhs, rhs));
        } else {
            ASSIGN(node, suffix());
        }

        return node;
    }

    NklAstNode_T suffix() {
        DEFINE(node, term());

        while (true) {
            if (accept(t_period_aster)) {
                node = m_ast.make_deref(node);
            } else if (accept(t_par_l)) {
                std::vector<NklAstNode_T> args{};
                CHECK(parseArgs(
                    args,
                    t_par_r,
                    {
                        .allow_trailing_comma = true,
                    }));
                node = m_ast.make_call(node, args);
            } else if (m_cur_expr_kind == Expr_Regular && accept(t_brace_l)) {
                std::vector<NklAstNode_T> args{};
                CHECK(parseArgs(
                    args,
                    t_brace_r,
                    {
                        .allow_trailing_comma = true,
                    }));
                node = m_ast.make_object_literal(node, args);
            } else if (accept(t_bracket_l)) {
                ASSIGN(node, m_ast.make_index(node, check(t_bracket_r) ? NklAstNode_T{} : expr()));
                EXPECT(t_bracket_r);
            } else if (accept(t_period)) {
                DEFINE(name, identifier());
                node = m_ast.make_member(node, name);
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T term() {
        NklAstNode_T node;

        if (check(t_int)) {
            NK_LOG_DBG("accept(int, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_numeric_int(m_cur_token);
            getToken();
        } else if (check(t_float)) {
            NK_LOG_DBG("accept(float, \"%.*s\"", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_numeric_float(m_cur_token);
            getToken();
        } else if (check(t_string)) {
            NK_LOG_DBG("accept(string, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_string_literal(m_cur_token);
            getToken();
        } else if (check(t_escaped_string)) {
            NK_LOG_DBG(
                "accept(escaped_string, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_escaped_string_literal(m_cur_token);
            getToken();
        }

        else if (check(t_id)) {
            ASSIGN(node, m_ast.make_id(identifier()));
        } else if (check(t_intrinsic)) {
            ASSIGN(node, m_ast.make_intrinsic(m_cur_token));
            getToken();
        }

        else if (accept(t_u8)) {
            node = m_ast.make_u8();
        } else if (accept(t_u16)) {
            node = m_ast.make_u16();
        } else if (accept(t_u32)) {
            node = m_ast.make_u32();
        } else if (accept(t_u64)) {
            node = m_ast.make_u64();
        } else if (accept(t_i8)) {
            node = m_ast.make_i8();
        } else if (accept(t_i16)) {
            node = m_ast.make_i16();
        } else if (accept(t_i32)) {
            node = m_ast.make_i32();
        } else if (accept(t_i64)) {
            node = m_ast.make_i64();
        } else if (accept(t_f32)) {
            node = m_ast.make_f32();
        } else if (accept(t_f64)) {
            node = m_ast.make_f64();
        }

        else if (accept(t_any_t)) {
            node = m_ast.make_any_t();
        } else if (accept(t_bool)) {
            node = m_ast.make_bool();
        } else if (accept(t_type_t)) {
            node = m_ast.make_type_t();
        } else if (accept(t_void)) {
            node = m_ast.make_void();
        }

        else if (accept(t_true)) {
            node = m_ast.make_true();
        } else if (accept(t_false)) {
            node = m_ast.make_false();
        }

        else if (accept(t_par_l)) {
            ParseFieldsResult res{};
            CHECK(parseFields(
                res,
                t_par_r,
                {
                    .accept_const = false,
                    .accept_init = true,
                    .allow_tuple_mode = true,
                }));
            if (accept(t_minus_greater)) {
                if (res.trailing_comma_parsed) {
                    return error("trailing comma is not allowed in function signature"),
                           NklAstNode_T{};
                }
                DEFINE(ret_type, expr(Expr_Type));
                ASSIGN(
                    node,
                    m_ast.make_fn(
                        res.fields,
                        ret_type,
                        m_cur_expr_kind == Expr_Regular && check(t_brace_l) ? block()
                                                                            : NklAstNode_T{},
                        res.variadic_parsed));
            } else {
                if (res.tuple_parsed) {
                    //@Todo Maybe avoid copy in case of tuple
                    std::vector<NklAstNode_T> nodes{};
                    nodes.reserve(res.fields.size);
                    for (auto const &field : res.fields) {
                        nodes.emplace_back(*field.type);
                    }
                    if (m_cur_expr_kind == Expr_Type) {
                        ASSIGN(node, m_ast.make_tuple_type(nodes));
                    } else {
                        ASSIGN(
                            node,
                            nodes.size() == 1 && !res.trailing_comma_parsed
                                ? nodes[0]
                                : m_ast.make_tuple(nodes));
                    }
                } else {
                    ASSIGN(node, m_ast.make_packed_struct(res.fields));
                }
            }
        }

        else if (accept(t_dollar)) {
            ASSIGN(node, m_ast.make_run(block()));
        }

        else if (check(t_eof)) {
            return error("unexpected end of file"), NklAstNode_T{};
        } else {
            return error("unexpected token '%.*s'", m_cur_token->text.size, m_cur_token->text.data),
                   NklAstNode_T{};
        }

        return node;
    }

    void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Parser error already initialized");
        m_error_occurred = true;
        va_list ap;
        va_start(ap, fmt);
        std::vprintf(fmt, ap);
        va_end(ap);
    }
#endif
};

} // namespace

NklAstNode nkl_parse(NklAst ast, std::vector<NklToken> const &tokens) {
    NK_LOG_TRC(__func__);

    assert(tokens.size() && "empty token array");

    ParseEngine engine{tokens, ast};
    NklAstNode root = engine.parse();

    NK_LOG_INF(
        "root: %s", (char const *)[&]() {
            auto sb = nksb_create();
            nkl_ast_inspect(root, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }());

    if (engine.m_error_occurred) {
        std::abort(); // TODO Report errors properly
    }

    return root;
}
