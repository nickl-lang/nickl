#include "parser.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "ast_impl.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string.hpp"
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
    nkslice<NklToken const> m_tokens;
    NklAst m_ast;
    std::string &m_err_str;

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
        return nkl_pushNode(m_ast, block(false)).data;
    }

private:
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
            // TODO Improve token quote for string constants etc.
            return error(
                "expected `%s` before `%.*s`",
                s_token_text[id],
                m_cur_token->text.size,
                m_cur_token->text.data);
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
        field.id = cs2nkid("field");
        if (cfg.accept_const && accept(t_const)) {
            field.id = cs2nkid("const_field");
            tuple_mode = false;
        }
        if (tuple_mode) {
            if (check(t_id)) {
                ASSIGN(field.args[0], nkl_pushNode(m_ast, nkl_makeNode0(n_id, identifier())));
            }
            if (accept(t_colon)) {
                tuple_mode = false;
                ASSIGN(field.args[1], nkl_pushNode(m_ast, expr(Expr_Type)));
            } else {
                if (field.args[0].data) {
                    m_cur_token--;
                }
                ASSIGN(field.args[1], nkl_pushNode(m_ast, expr()));
            }
        } else {
            ASSIGN(field.args[0], nkl_pushNode(m_ast, nkl_makeNode0(n_id, identifier())));
            EXPECT(t_colon);
            ASSIGN(field.args[1], nkl_pushNode(m_ast, expr(Expr_Type)));
            if (cfg.accept_init && accept(t_eq)) {
                ASSIGN(field.args[2], nkl_pushNode(m_ast, expr()));
            }
        }
        if (tuple_mode) {
            while (accept(t_comma)) {
                if (check(closer)) {
                    res.trailing_comma_parsed = true;
                    break;
                }
                NklAstNode_T &field = res.fields.emplace_back();
                ASSIGN(field.args[1], nkl_pushNode(m_ast, expr()));
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
                field.id = cs2nkid("field");
                if (cfg.accept_const && accept(t_const)) {
                    field.id = cs2nkid("const_field");
                }
                ASSIGN(field.args[0], nkl_pushNode(m_ast, nkl_makeNode0(n_id, identifier())));
                EXPECT(t_colon);
                ASSIGN(field.args[1], nkl_pushNode(m_ast, expr(Expr_Type)));
                if (cfg.accept_init && accept(t_eq)) {
                    ASSIGN(field.args[2], nkl_pushNode(m_ast, expr()));
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
        if (accept(closer)) {
            return {};
        }
        do {
            if (cfg.allow_trailing_comma && check(closer)) {
                break;
            }
            NklAstNode_T &arg = args.emplace_back();
            arg.id = cs2nkid("arg");
            if (check(t_id)) {
                NK_LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
                auto id = m_cur_token;
                getToken();
                if (accept(t_eq)) {
                    arg.args[0] = nkl_pushNode(m_ast, nkl_makeNode0(n_id, id));
                } else {
                    m_cur_token--;
                }
            }
            ASSIGN(arg.args[1], nkl_pushNode(m_ast, expr()));
        } while (accept(t_comma));
        EXPECT(closer);
        return {};
    }

    NklAstNode_T block(bool capture_brace = true) {
        // TODO Optimize node allocation in parser for nodes
        std::vector<NklAstNode_T> nodes{};

        auto _n_token = m_cur_token;
        bool expect_brace_r = capture_brace && accept(t_brace_l);

        while (!check(t_eof) && (!expect_brace_r || !check(t_brace_r))) {
            NK_LOG_DBG("next statement: token=" LOG_TOKEN(m_cur_token->id));
            APPEND(nodes, statement());
            NK_LOG_DBG("end of statement");
        }

        if (expect_brace_r) {
            EXPECT(t_brace_r);
        }

        auto node =
            nodes.size() == 0 ? nkl_makeNode0(n_nop, _n_token)
            : nodes.size() == 1
                ? nodes.front()
                : nkl_makeNode1(
                      n_block, _n_token, nkl_pushNodeAr(m_ast, {nodes.data(), nodes.size()}));

        return capture_brace ? nkl_makeNode1(n_scope, _n_token, nkl_pushNode(m_ast, node)) : node;
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

    NklAstNode_T statement() {
        NklAstNode_T node;
        auto _n_token = m_cur_token;

        if (accept(t_return)) {
            if (check(t_semi)) {
                node = nkl_makeNode0(n_return, _n_token);
            } else {
                DEFINE(arg, statement());
                node = nkl_makeNode1(n_return, _n_token, nkl_pushNode(m_ast, arg));
            }
        } else if (accept(t_break)) {
            node = nkl_makeNode0(n_break, _n_token);
        } else if (accept(t_continue)) {
            node = nkl_makeNode0(n_continue, _n_token);
        } else if (accept(t_if)) {
            DEFINE(cond, expr(Expr_Bool));
            DEFINE(then_body, statement());
            DEFINE(else_body, accept(t_else) ? statement() : NklAstNode_T{});
            node = nkl_makeNode3(
                n_if,
                _n_token,
                nkl_pushNode(m_ast, cond),
                nkl_pushNode(m_ast, then_body),
                nkl_pushNode(m_ast, else_body));
        } else if (accept(t_while)) {
            DEFINE(cond, expr(Expr_Bool));
            DEFINE(body, statement());
            node = nkl_makeNode2(
                n_while, _n_token, nkl_pushNode(m_ast, cond), nkl_pushNode(m_ast, body));
        } else if (check(t_brace_l)) {
            ASSIGN(node, block());
        } else if (accept(t_defer)) {
            ASSIGN(node, nkl_makeNode1(n_defer_stmt, _n_token, nkl_pushNode(m_ast, assignment())));
        } else if (accept(t_import)) {
            ASSIGN(
                node,
                nkl_makeNode1(
                    n_import, _n_token, nkl_pushNode(m_ast, nkl_makeNode0(n_id, identifier()))));
            // } else if (accept(t_for)) {
            //     DEFINE(it, identifier());
            //     bool const by_ptr = accept(t_period_aster);
            //     EXPECT(t_in);
            //     DEFINE(range, expr(Expr_Bool)); //@Todo Using bool for range in for
            //     DEFINE(body, statement());
            //     if (by_ptr) {
            //         ASSIGN(node, m_ast.make_for_by_ptr(it, range, body));
            //     } else {
            //         ASSIGN(node, m_ast.make_for(it, range, body));
            //     }
        } else if (check(t_tag)) {
            //@Todo Refactor token debug prints
            NK_LOG_DBG("accept(tag, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data);
            auto tag = m_cur_token;
            getToken();
            if (accept(t_colon_2x)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_tag_def,
                        _n_token,
                        nkl_pushNode(m_ast, nkl_makeNode0(n_id, tag)),
                        nkl_pushNode(m_ast, tuple())));
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
                ASSIGN(
                    node,
                    nkl_makeNode3(
                        n_tag,
                        _n_token,
                        nkl_pushNode(m_ast, nkl_makeNode0(n_name, tag)),
                        nkl_pushNodeAr(m_ast, {args.data(), args.size()}),
                        nkl_pushNode(m_ast, statement())));
            }
        } else if (accept(t_const)) {
            //@Boilerplate in const var decl
            DEFINE(name, nkl_makeNode0(n_id, identifier()));
            DEFINE(type, expr(Expr_Type));
            DEFINE(value, accept(t_eq) ? tuple() : NklAstNode_T{});
            node = nkl_makeNode3(
                n_var_decl,
                _n_token,
                nkl_pushNode(m_ast, name),
                nkl_pushNode(m_ast, type),
                nkl_pushNode(m_ast, value));
        } else {
            ASSIGN(node, assignment());
        }

        //@Todo Hack for id-statements, also support multiple assignment
        if (node.id == cs2nkid("id")) {
            auto _n_token = m_cur_token;

            if (accept(t_colon_eq)) {
                DEFINE(rhs, tuple());
                node = nkl_makeNode2(
                    n_define, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, rhs));
            } else if (accept(t_colon_2x)) {
                NklAstNode_T rhs{};
                // if (accept(t_import)) {
                //     ASSIGN(rhs, parseImport());
                // } else {
                ASSIGN(rhs, tuple());
                // }
                node = nkl_makeNode2(
                    n_comptime_const_def,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, rhs));
            } else if (accept(t_colon)) {
                DEFINE(type, expr(Expr_Type));
                DEFINE(value, accept(t_eq) ? tuple() : NklAstNode_T{});
                node = nkl_makeNode3(
                    n_var_decl,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, type),
                    nkl_pushNode(m_ast, value));
            }
        }

        if (m_last_token->id != t_semi && m_last_token->id != t_brace_r &&
            m_last_token->id != t_brace_2x) {
            EXPECT(t_semi);
        }
        while (accept(t_semi)) {
        }

        return node;
    }

    NklAstNode_T assignment() {
        DEFINE(node, tuple());
        auto _n_token = m_cur_token;

        if (accept(t_eq)) {
            //@Todo Support multiple assignment in parser
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_assign, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, logicOr())));
        } else if (accept(t_plus_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_add_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_minus_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_sub_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_aster_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_mul_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_slash_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_div_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_percent_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_mod_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_less_2x_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_lsh_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_greater_2x_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_rsh_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_bar_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_bitor_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_caret_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_xor_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        } else if (accept(t_amper_eq)) {
            ASSIGN(
                node,
                nkl_makeNode2(
                    n_bitand_assign,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, tuple())));
        }

        return node;
    }

    NklAstNode_T tuple() {
        auto _n_token = m_cur_token;
        bool trailing_comma_provided = false;
        bool all_ids = false;
        std::vector<NklAstNode_T> nodes{};
        CHECK(sequence(nodes, &trailing_comma_provided, &all_ids));
        return (nodes.size() > 1 || trailing_comma_provided)
                   ? nkl_makeNode1(
                         n_tuple, _n_token, nkl_pushNodeAr(m_ast, {nodes.data(), nodes.size()}))
                   : nodes.front();
    }

    NklAstNode_T expr(EExprKind kind = Expr_Regular) {
        auto const old_expr_kind = m_cur_expr_kind;
        m_cur_expr_kind = kind;
        defer {
            m_cur_expr_kind = old_expr_kind;
        };

        NklAstNode_T node;
        auto _n_token = m_cur_token;

        if (accept(t_struct)) {
            ParseFieldsResult res{};
            if (!accept(t_brace_2x)) {
                EXPECT(t_brace_l);
                CHECK(parseFields(
                    res,
                    t_brace_r,
                    {
                        .accept_const = true,
                        .accept_init = true,
                        .allow_tuple_mode = false,
                    }));
            }
            node = nkl_makeNode1(
                n_struct, _n_token, nkl_pushNodeAr(m_ast, {res.fields.data(), res.fields.size()}));
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
            node = nkl_makeNode1(
                n_union, _n_token, nkl_pushNodeAr(m_ast, {res.fields.data(), res.fields.size()}));
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
            node = nkl_makeNode1(
                n_enum, _n_token, nkl_pushNodeAr(m_ast, {res.fields.data(), res.fields.size()}));
        } else if (check(t_tag) && std_view(m_cur_token->text) == "#type") {
            getToken();
            ASSIGN(node, expr(Expr_Type));
        } else {
            ASSIGN(node, ternary());
        }

        return node;
    }

    NklAstNode_T ternary() {
        DEFINE(node, logicOr());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_question)) {
                DEFINE(then_body, logicOr());
                EXPECT(t_colon);
                DEFINE(else_body, logicOr());
                node = nkl_makeNode3(
                    n_ternary,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, then_body),
                    nkl_pushNode(m_ast, else_body));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T logicOr() {
        DEFINE(node, logicAnd());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_bar_2x)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_or, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, logicOr())));

            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T logicAnd() {
        DEFINE(node, bitOr());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_amper_2x)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_and, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, bitOr())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T bitOr() {
        DEFINE(node, bitXor());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_bar)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_bitor,
                        _n_token,
                        nkl_pushNode(m_ast, node),
                        nkl_pushNode(m_ast, bitXor())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T bitXor() {
        DEFINE(node, bitAnd());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_caret)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_xor, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, bitAnd())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T bitAnd() {
        DEFINE(node, equality());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_amper)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_bitand,
                        _n_token,
                        nkl_pushNode(m_ast, node),
                        nkl_pushNode(m_ast, equality())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T equality() {
        DEFINE(node, relation());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_eq_2x)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_eq,
                        _n_token,
                        nkl_pushNode(m_ast, node),
                        nkl_pushNode(m_ast, relation())));
            } else if (accept(t_exclam_eq)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_ne,
                        _n_token,
                        nkl_pushNode(m_ast, node),
                        nkl_pushNode(m_ast, relation())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T relation() {
        DEFINE(node, shift());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_less)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_lt, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, shift())));
            } else if (accept(t_less_eq)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_le, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, shift())));
            } else if (accept(t_greater)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_gt, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, shift())));
            } else if (accept(t_greater_eq)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_ge, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, shift())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T shift() {
        DEFINE(node, addSub());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_less_2x)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_lsh, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, addSub())));
            } else if (accept(t_greater_2x)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_rsh, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, addSub())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T addSub() {
        DEFINE(node, mulDiv());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_plus)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_add, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, mulDiv())));
            } else if (accept(t_minus)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_sub, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, mulDiv())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T mulDiv() {
        DEFINE(node, prefix());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_aster)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_mul, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, prefix())));
            } else if (accept(t_slash)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_div, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, prefix())));
            } else if (accept(t_percent)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_mod, _n_token, nkl_pushNode(m_ast, node), nkl_pushNode(m_ast, prefix())));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T prefix() {
        NklAstNode_T node;
        auto _n_token = m_cur_token;

        if (accept(t_plus)) {
            ASSIGN(node, nkl_makeNode1(n_uplus, _n_token, nkl_pushNode(m_ast, prefix())));
        } else if (accept(t_minus)) {
            ASSIGN(node, nkl_makeNode1(n_uminus, _n_token, nkl_pushNode(m_ast, prefix())));
        } else if (accept(t_exclam)) {
            ASSIGN(node, nkl_makeNode1(n_not, _n_token, nkl_pushNode(m_ast, prefix())));
        } else if (accept(t_tilde)) {
            ASSIGN(node, nkl_makeNode1(n_compl, _n_token, nkl_pushNode(m_ast, prefix())));
        } else if (accept(t_amper)) {
            ASSIGN(node, nkl_makeNode1(n_addr, _n_token, nkl_pushNode(m_ast, prefix())));
        } else if (accept(t_aster)) {
            if (accept(t_const)) {
                ASSIGN(
                    node, nkl_makeNode1(n_const_ptr_type, _n_token, nkl_pushNode(m_ast, prefix())));
            } else {
                ASSIGN(node, nkl_makeNode1(n_ptr_type, _n_token, nkl_pushNode(m_ast, prefix())));
            }
        } else if (accept(t_bracket_2x)) {
            ASSIGN(node, nkl_makeNode1(n_slice_type, _n_token, nkl_pushNode(m_ast, prefix())));
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
                node = nkl_makeNode1(
                    n_array, _n_token, nkl_pushNodeAr(m_ast, {nodes.data(), nodes.size()}));
            } else {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_array_type,
                        _n_token,
                        nkl_pushNode(m_ast, prefix()),
                        nkl_pushNode(m_ast, nodes.front())));
            }
        } else if (accept(t_cast)) {
            EXPECT(t_par_l);
            DEFINE(lhs, expr(Expr_Type));
            EXPECT(t_par_r);
            DEFINE(rhs, prefix());
            node =
                nkl_makeNode2(n_cast, _n_token, nkl_pushNode(m_ast, lhs), nkl_pushNode(m_ast, rhs));
        } else {
            ASSIGN(node, suffix());
        }

        return node;
    }

    NklAstNode_T suffix() {
        DEFINE(node, term());

        while (true) {
            auto _n_token = m_cur_token;
            if (accept(t_period_aster)) {
                node = nkl_makeNode1(n_deref, _n_token, nkl_pushNode(m_ast, node));
            } else if (accept(t_par_l)) {
                std::vector<NklAstNode_T> args{};
                CHECK(parseArgs(
                    args,
                    t_par_r,
                    {
                        .allow_trailing_comma = true,
                    }));
                node = nkl_makeNode2(
                    n_call,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNodeAr(m_ast, {args.data(), args.size()}));
            } else if (m_cur_expr_kind == Expr_Regular && accept(t_brace_l)) {
                std::vector<NklAstNode_T> args{};
                CHECK(parseArgs(
                    args,
                    t_brace_r,
                    {
                        .allow_trailing_comma = true,
                    }));
                node = nkl_makeNode2(
                    n_object_literal,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNodeAr(m_ast, {args.data(), args.size()}));
            } else if (accept(t_bracket_l)) {
                ASSIGN(
                    node,
                    nkl_makeNode2(
                        n_index,
                        _n_token,
                        nkl_pushNode(m_ast, node),
                        nkl_pushNode(m_ast, check(t_bracket_r) ? NklAstNode_T{} : expr())));
                EXPECT(t_bracket_r);
            } else if (accept(t_period)) {
                DEFINE(name, identifier());
                node = nkl_makeNode2(
                    n_member,
                    _n_token,
                    nkl_pushNode(m_ast, node),
                    nkl_pushNode(m_ast, nkl_makeNode0(n_id, name)));
            } else {
                break;
            }
        }

        return node;
    }

    NklAstNode_T term() {
        NklAstNode_T node;
        auto _n_token = m_cur_token;

        if (accept(t_int)) {
            NK_LOG_DBG("accept(int, \"%.*s\")", _n_token->text.size, _n_token->text.data);
            node = nkl_makeNode0(n_int, _n_token);
        } else if (accept(t_float)) {
            NK_LOG_DBG("accept(float, \"%.*s\"", _n_token->text.size, _n_token->text.data);
            node = nkl_makeNode0(n_float, _n_token);
        } else if (accept(t_string)) {
            NK_LOG_DBG("accept(string, \"%.*s\")", _n_token->text.size, _n_token->text.data);
            node = nkl_makeNode0(n_string, _n_token);
        } else if (accept(t_escaped_string)) {
            NK_LOG_DBG(
                "accept(escaped_string, \"%.*s\")", _n_token->text.size, _n_token->text.data);
            node = nkl_makeNode0(n_escaped_string, _n_token);
        }

        else if (check(t_id)) {
            ASSIGN(node, nkl_makeNode0(n_id, identifier()));
        } else if (accept(t_intrinsic)) {
            node = nkl_makeNode0(n_intrinsic, _n_token);
        }

        else if (accept(t_u8)) {
            node = nkl_makeNode0(n_u8, _n_token);
        } else if (accept(t_u16)) {
            node = nkl_makeNode0(n_u16, _n_token);
        } else if (accept(t_u32)) {
            node = nkl_makeNode0(n_u32, _n_token);
        } else if (accept(t_u64)) {
            node = nkl_makeNode0(n_u64, _n_token);
        } else if (accept(t_i8)) {
            node = nkl_makeNode0(n_i8, _n_token);
        } else if (accept(t_i16)) {
            node = nkl_makeNode0(n_i16, _n_token);
        } else if (accept(t_i32)) {
            node = nkl_makeNode0(n_i32, _n_token);
        } else if (accept(t_i64)) {
            node = nkl_makeNode0(n_i64, _n_token);
        } else if (accept(t_f32)) {
            node = nkl_makeNode0(n_f32, _n_token);
        } else if (accept(t_f64)) {
            node = nkl_makeNode0(n_f64, _n_token);
        }

        else if (accept(t_any_t)) {
            node = nkl_makeNode0(n_any_t, _n_token);
        } else if (accept(t_bool)) {
            node = nkl_makeNode0(n_bool, _n_token);
        } else if (accept(t_type_t)) {
            node = nkl_makeNode0(n_type_t, _n_token);
        } else if (accept(t_void)) {
            node = nkl_makeNode0(n_void, _n_token);
        }

        else if (accept(t_true)) {
            node = nkl_makeNode0(n_true, _n_token);
        } else if (accept(t_false)) {
            node = nkl_makeNode0(n_false, _n_token);
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
            if (check(t_minus_greater) || (!res.tuple_parsed && check(t_brace_l))) {
                NklAstNode_T ret_type{};
                if (accept(t_minus_greater)) {
                    ASSIGN(ret_type, expr(Expr_Type));
                }
                if (m_cur_expr_kind == Expr_Regular && check(t_brace_l)) {
                    DEFINE(body, block());
                    node = nkl_makeNode3(
                        res.variadic_parsed ? n_fn_var : n_fn,
                        _n_token,
                        nkl_pushNodeAr(m_ast, {res.fields.data(), res.fields.size()}),
                        ret_type.id ? nkl_pushNode(m_ast, ret_type) : NklAstNodeArray{},
                        nkl_pushNode(m_ast, body));
                } else {
                    node = nkl_makeNode2(
                        res.variadic_parsed ? n_fn_type_var : n_fn_type,
                        _n_token,
                        nkl_pushNodeAr(m_ast, {res.fields.data(), res.fields.size()}),
                        ret_type.id ? nkl_pushNode(m_ast, ret_type) : NklAstNodeArray{});
                }
            } else {
                if (res.tuple_parsed) {
                    //@Todo Maybe avoid copy in case of tuple
                    std::vector<NklAstNode_T> nodes{};
                    nodes.reserve(res.fields.size());
                    for (auto const &field : res.fields) {
                        nodes.emplace_back(*field.args[1].data);
                    }
                    if (m_cur_expr_kind == Expr_Type) {
                        node = nkl_makeNode1(
                            n_tuple_type,
                            _n_token,
                            nkl_pushNodeAr(m_ast, {nodes.data(), nodes.size()}));
                    } else {
                        node = nodes.size() == 1 && !res.trailing_comma_parsed
                                   ? nodes.front()
                                   : nkl_makeNode1(
                                         n_tuple,
                                         _n_token,
                                         nkl_pushNodeAr(m_ast, {nodes.data(), nodes.size()}));
                    }
                } else {
                    node = nkl_makeNode1(
                        n_packed_struct,
                        _n_token,
                        nkl_pushNodeAr(m_ast, {res.fields.data(), res.fields.size()}));
                }
            }
        }

        else if (accept(t_dollar)) {
            ASSIGN(node, nkl_makeNode1(n_run, _n_token, nkl_pushNode(m_ast, block())));
        }

        else if (check(t_eof)) {
            m_cur_token--;
            return error("unexpected end of file"), NklAstNode_T{};
        } else {
            return error("unexpected token `%.*s`", m_cur_token->text.size, m_cur_token->text.data),
                   NklAstNode_T{};
        }

        return node;
    }

    void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Parser error already initialized");

        va_list ap;
        va_start(ap, fmt);
        m_err_str = string_vformat(fmt, ap);
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

NklAstNode nkl_parse(
    NklAst ast,
    nkslice<NklToken const> tokens,
    std::string &err_str,
    NklTokenRef &err_token) {
    EASY_FUNCTION(::profiler::colors::Teal200);
    NK_LOG_TRC(__func__);

    assert(tokens.size() && "empty token array");

    ParseEngine engine{tokens, ast, err_str};
    NklAstNode root = engine.parse();

    NK_LOG_INF(
        "root: %s", (char const *)[&]() {
            auto sb = nksb_create();
            nkl_inspectNode(root, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }());

    if (engine.m_error_occurred) {
        err_token = engine.m_cur_token;
        return nullptr;
    }

    return root;
}
