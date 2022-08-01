#include "nkl/lang/parser.hpp"

#include <cassert>
#include <cstdio>

#include "nk/ds/array.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nkl/lang/tokens.hpp"

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_token_id[ID], s_token_text[ID]

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(*AR.push() = (VAL))
#define EXPECT(ID) CHECK(expect(ID))

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::parser);

struct ParseEngine {
    TokenArray const &m_tokens;
    LangAst &m_ast;
    StringBuilder &m_err;

    bool m_error_occurred = false;

    TokenRef m_cur_token{};
    TokenRef m_last_token{};

    enum EExprKind {
        Expr_Regular,
        Expr_Bool,
        Expr_Type,
    };

    EExprKind m_cur_expr_kind = Expr_Regular;

    NodeRef parse() {
        EASY_FUNCTION(::profiler::colors::Green200);
        LOG_TRC(__func__);

        assert(m_tokens.size && m_tokens.back().id == t_eof && "ill-formed token stream");

        m_cur_token = m_tokens.begin();
        return m_ast.gen(block(false));
    }

private:
    struct Void {};

    void getToken() {
        assert(m_cur_token->id != t_eof);
        m_last_token = m_cur_token++;

        LOG_DBG("next token: " LOG_TOKEN(m_cur_token->id));
    }

    bool accept(ETokenID id) {
        if (check(id)) {
            LOG_DBG("accept" LOG_TOKEN(id));
            getToken();
            return true;
        }
        return false;
    }

    bool check(ETokenID id) const {
        return m_cur_token->id == id;
    }

    void expect(ETokenID id) {
        if (!accept(id)) {
            return error("'%s' expected", s_token_text[id]);
        }
    }

    Void sequence(
        Array<Node> &nodes,
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
            if (all_ids && nodes.back().id != Node_id) {
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
        Array<FieldNode> fields;
        bool trailing_comma_parsed;
        bool variadic_parsed;
        bool tuple_parsed;
    };

    Void parseFields(ParseFieldsResult &res, ETokenID closer, ParseFieldsConfig const &cfg) {
        bool &tuple_mode = res.tuple_parsed = cfg.allow_tuple_mode;
        if (accept(closer)) {
            return {};
        }
        FieldNode &field = *res.fields.push() = {};
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
                ASSIGN(field.type, m_ast.gen(expr(Expr_Type)));
            } else {
                if (field.name) {
                    m_cur_token--;
                }
                ASSIGN(field.type, m_ast.gen(expr()));
            }
        } else {
            ASSIGN(field.name, identifier());
            EXPECT(t_colon);
            ASSIGN(field.type, m_ast.gen(expr(Expr_Type)));
            if (cfg.accept_init && accept(t_eq)) {
                ASSIGN(field.init_value, m_ast.gen(expr()));
            }
        }
        if (tuple_mode) {
            while (accept(t_comma)) {
                if (check(closer)) {
                    res.trailing_comma_parsed = true;
                    break;
                }
                FieldNode &field = *res.fields.push() = {};
                ASSIGN(field.type, m_ast.gen(expr()));
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
                FieldNode &field = *res.fields.push() = {};
                if (cfg.accept_const && accept(t_const)) {
                    field.is_const = true;
                }
                ASSIGN(field.name, identifier());
                EXPECT(t_colon);
                ASSIGN(field.type, m_ast.gen(expr(Expr_Type)));
                if (cfg.accept_init && accept(t_eq)) {
                    ASSIGN(field.init_value, m_ast.gen(expr()));
                }
            }
        }
        EXPECT(closer);
        return {};
    }

    struct ParseArgsConfig {
        bool allow_trailing_comma;
    };

    Void parseArgs(Array<NamedNode> &args, ETokenID closer, ParseArgsConfig const &cfg) {
        do {
            if (cfg.allow_trailing_comma && check(closer)) {
                break;
            }
            NamedNode &arg = *args.push() = {};
            if (check(t_id)) {
                LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
                auto id = m_cur_token;
                getToken();
                if (accept(t_eq)) {
                    arg.name = id;
                } else {
                    m_cur_token--;
                }
            }
            ASSIGN(arg.node, m_ast.gen(expr()));
        } while (accept(t_comma));
        EXPECT(closer);
        return {};
    }

    Node block(bool capture_brace = true) {
        //@Refactor Optimize node allocation in parser for nodes
        Array<Node> nodes{};
        defer {
            nodes.deinit();
        };

        bool expect_brace_r = capture_brace && accept(t_brace_l);

        while (!check(t_eof) && (!expect_brace_r || !check(t_brace_r))) {
            LOG_DBG("next statement: token=" LOG_TOKEN(m_cur_token->id));
            APPEND(nodes, statement());
            LOG_DBG("end of statement");
        }

        if (expect_brace_r) {
            EXPECT(t_brace_r);
        }

        auto node = nodes.size == 0   ? m_ast.make_nop()
                    : nodes.size == 1 ? nodes.front()
                                      : m_ast.make_block(nodes.slice());

        return capture_brace ? m_ast.make_scope(node) : node;
    }

    TokenRef identifier() {
        if (!check(t_id)) {
            return error("identifier expected"), TokenRef{};
        }
        LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
        auto id = m_cur_token;
        getToken();
        return id;
    }

    Node parseImport() {
        Node node{};
        if (check(t_str_const)) {
            LOG_DBG("accept(str_const, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            ASSIGN(node, m_ast.make_import_path(m_cur_token))
            getToken();
        } else {
            Array<TokenRef> tokens{};
            defer {
                tokens.deinit();
            };
            APPEND(tokens, identifier());
            while (accept(t_period)) {
                APPEND(tokens, identifier());
            }
            ASSIGN(node, m_ast.make_import(tokens));
        }
        return node;
    }

    Node statement() {
        Node node;

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
            DEFINE(else_body, accept(t_else) ? statement() : Node{});
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
            LOG_DBG("accept(tag, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            auto tag = m_cur_token;
            getToken();
            if (accept(t_colon_2x)) {
                ASSIGN(node, m_ast.make_tag_def(tag, tuple()));
            } else {
                Array<NamedNode> args{};
                defer {
                    args.deinit();
                };
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
            DEFINE(value, accept(t_eq) ? tuple() : Node{});
            ASSIGN(node, m_ast.make_const_decl(name, type, value));
        } else {
            ASSIGN(node, assignment());
        }

        //@Todo Hack for id-statements, also support multiple assignment
        if (node.id == nodeId(Node_id)) {
            if (accept(t_colon_eq)) {
                ASSIGN(node, m_ast.make_define({&Node_token_value(&node), 1}, tuple()));
            } else if (accept(t_colon_2x)) {
                Node rhs{};
                if (accept(t_import)) {
                    ASSIGN(rhs, parseImport());
                } else {
                    ASSIGN(rhs, tuple());
                }
                ASSIGN(node, m_ast.make_comptime_const_def(Node_token_value(&node), rhs));
            } else if (accept(t_colon)) {
                DEFINE(type, expr(Expr_Type));
                DEFINE(value, accept(t_eq) ? tuple() : Node{});
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

    Node assignment() {
        DEFINE(node, tuple());

        if (accept(t_eq)) {
            //@Todo Support multiple assignment in parser
            ASSIGN(node, m_ast.make_assign({m_ast.gen(node), 1}, tuple()));
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

    Node tuple() {
        bool trailing_comma_provided = false;
        bool all_ids = false;
        Array<Node> nodes{};
        defer {
            nodes.deinit();
        };
        CHECK(sequence(nodes, &trailing_comma_provided, &all_ids));
        defer {
            nodes.deinit();
        };
        return (nodes.size > 1 || trailing_comma_provided) ? m_ast.make_tuple(nodes)
                                                           : nodes.front();
    }

    Node expr(EExprKind kind = Expr_Regular) {
        auto const old_expr_kind = m_cur_expr_kind;
        m_cur_expr_kind = kind;
        defer {
            m_cur_expr_kind = old_expr_kind;
        };

        Node node;

        if (accept(t_struct)) {
            EXPECT(t_brace_l);
            ParseFieldsResult res{};
            defer {
                res.fields.deinit();
            };
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
            defer {
                res.fields.deinit();
            };
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
            defer {
                res.fields.deinit();
            };
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

    Node ternary() {
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

    Node logicOr() {
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

    Node logicAnd() {
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

    Node bitOr() {
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

    Node bitXor() {
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

    Node bitAnd() {
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

    Node equality() {
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

    Node relation() {
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

    Node shift() {
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

    Node addSub() {
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

    Node mulDiv() {
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

    Node prefix() {
        Node node;

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
            Array<Node> nodes{};
            defer {
                nodes.deinit();
            };
            CHECK(sequence(nodes, &trailing_comma_provided, &all_ids));
            defer {
                nodes.deinit();
            };
            EXPECT(t_bracket_r);
            if (nodes.size == 0) {
                return error("TODO ERR empty array"), Node{};
            }
            if (nodes.size > 1 || check(t_par_r) || check(t_bracket_r) || check(t_bracket_r) ||
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

    Node suffix() {
        DEFINE(node, term());

        while (true) {
            if (accept(t_period_aster)) {
                node = m_ast.make_deref(node);
            } else if (accept(t_par_l)) {
                Array<NamedNode> args{};
                defer {
                    args.deinit();
                };
                CHECK(parseArgs(
                    args,
                    t_par_r,
                    {
                        .allow_trailing_comma = true,
                    }));
                node = m_ast.make_call(node, args);
            } else if (m_cur_expr_kind == Expr_Regular && accept(t_brace_l)) {
                Array<NamedNode> args{};
                defer {
                    args.deinit();
                };
                CHECK(parseArgs(
                    args,
                    t_brace_r,
                    {
                        .allow_trailing_comma = true,
                    }));
                node = m_ast.make_object_literal(node, args);
            } else if (accept(t_bracket_l)) {
                ASSIGN(node, m_ast.make_index(node, check(t_bracket_r) ? Node{} : expr()));
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

    Node term() {
        Node node;

        if (check(t_int_const)) {
            LOG_DBG("accept(int_const, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_numeric_int(m_cur_token);
            getToken();
        } else if (check(t_float_const)) {
            LOG_DBG("accept(float_const, \"%.*s\"", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_numeric_float(m_cur_token);
            getToken();
        } else if (check(t_str_const)) {
            LOG_DBG("accept(str_const, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            node = m_ast.make_string_literal(m_cur_token);
            getToken();
        } else if (check(t_escaped_str_const)) {
            LOG_DBG(
                "accept(escaped_str_const, \"%.*s\")",
                m_cur_token->text.size,
                m_cur_token->text.data);
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
            defer {
                res.fields.deinit();
            };
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
                    return error("trailing comma is not allowed in function signature"), Node{};
                }
                DEFINE(ret_type, expr(Expr_Type));
                ASSIGN(
                    node,
                    m_ast.make_fn(
                        res.fields,
                        ret_type,
                        m_cur_expr_kind == Expr_Regular && check(t_brace_l) ? block() : Node{},
                        res.variadic_parsed));
            } else {
                if (res.tuple_parsed) {
                    //@Todo Maybe avoid copy in case of tuple
                    Array<Node> nodes{};
                    defer {
                        nodes.deinit();
                    };
                    nodes.reserve(res.fields.size);
                    for (auto const &field : res.fields) {
                        *nodes.push() = *field.type;
                    }
                    if (m_cur_expr_kind == Expr_Type) {
                        ASSIGN(node, m_ast.make_tuple_type(nodes));
                    } else {
                        ASSIGN(
                            node,
                            nodes.size == 1 && !res.trailing_comma_parsed
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
            return error("unexpected end of file"), Node{};
        } else {
            return error("unexpected token '%.*s'", m_cur_token->text.size, m_cur_token->text.data),
                   Node{};
        }

        return node;
    }

    void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Parser error already initialized");
        m_error_occurred = true;
        va_list ap;
        va_start(ap, fmt);
        m_err.vprintf(fmt, ap);
        va_end(ap);
    }
};

} // namespace

bool Parser::parse(TokenArray const &tokens) {
    assert(tokens.size && "empty token array");

    ast.init();

    root = n_none;

    ParseEngine engine{tokens, ast, err};
    root = engine.parse();

#ifdef ENABLE_LOGGING
    //@Todo Use some existing allocator for printing?
    StackAllocator allocator{};
    DynamicStringBuilder sb{};
    defer {
        sb.deinit();
        allocator.deinit();
    };
#endif // ENABLE_LOGGING
    LOG_INF("root: %s", [&]() {
        return ast_inspect(root, sb).moveStr(allocator).data;
    }());

    return !engine.m_error_occurred;
}

} // namespace nkl
