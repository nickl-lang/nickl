#include "nkl/core/parser.hpp"

#include <cassert>
#include <cstdio>

#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::parser)

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_token_id[ID], s_token_text[ID]

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))

#define ASSIGN(SLOT, VAL)  \
    {                      \
        DEFINE(__val, VAL) \
        SLOT = __val;      \
    }

#define APPEND(AR, VAL)    \
    {                      \
        DEFINE(__val, VAL) \
        AR.push() = __val; \
    }

#define EXPECT(ID) CHECK(expect(ID))

struct ParseEngine {
    TokenArray const &m_tokens;
    Ast &m_ast;
    node_ref_t &m_root;
    string &m_err;

    bool m_error_occurred = false;

    Token const *m_cur_token{};

    void parse() {
        EASY_FUNCTION(::profiler::colors::Green200);

        m_cur_token = m_tokens.begin();
        m_root = m_ast.push(block(false));
    }

private:
    void getToken() {
        assert(m_cur_token->id != Token_eof);
        m_cur_token++;

        LOG_DBG("getToken(): " LOG_TOKEN(m_cur_token->id))
    }

    bool accept(ETokenID id) {
        if (check(id)) {
            LOG_DBG("accept" LOG_TOKEN(id))
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

    Array<Node> sequence(bool *allow_trailing_comma = nullptr) {
        Array<Node> nodes{};
        do {
            if (check(Token_par_r) && allow_trailing_comma) {
                *allow_trailing_comma = true;
                break;
            }
            APPEND(nodes, expr());
        } while (accept(Token_comma));
        return nodes;
    }

    Node block(bool capture_brace = true) {
        //@Refactor Use tmp_allocator in parser for nodes
        Array<Node> nodes{};
        DEFER({ nodes.deinit(); })

        bool expect_brace_r = capture_brace && accept(Token_brace_l);

        while (!check(Token_eof) && (!expect_brace_r || !check(Token_brace_r))) {
            LOG_DBG("<next statement> token=" LOG_TOKEN(m_cur_token->id))
            APPEND(nodes, statement());
            LOG_DBG("<end of statement>")
        }

        if (expect_brace_r) {
            EXPECT(Token_brace_r);
        }

        return nodes.size == 0   ? m_ast.make_nop()
               : nodes.size == 1 ? nodes.front()
                                 : m_ast.make_block(nodes.slice());
    }

    NamedNode declaration() {
        if (!check(Token_id)) {
            return error("identifier expected"), NamedNode{};
        }
        NamedNode decl;
        ASSIGN(decl.name, identifier());
        EXPECT(Token_colon);
        ASSIGN(decl.node, m_ast.push(expr()));
        return decl;
    }

    Id identifier() {
        LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data)
        Id id = s2id(m_cur_token->text);
        getToken();
        return id;
    }

    Node statement() {
        Node node;

        bool expect_semi = true;

        if (accept(Token_return)) {
            if (check(Token_semi)) {
                node = m_ast.make_return(nullptr);
            } else {
                ASSIGN(node, m_ast.make_return(m_ast.push(tuple())));
            }
        } else if (accept(Token_break)) {
            node = m_ast.make_break();
        } else if (accept(Token_continue)) {
            node = m_ast.make_continue();
        } else if (accept(Token_if)) {
            DEFINE(cond, m_ast.push(logicOr()));
            DEFINE(then_body, m_ast.push(statement()));
            DEFINE(else_body, accept(Token_else) ? m_ast.push(statement()) : n_none_ref);
            node = m_ast.make_if(cond, then_body, else_body);
            expect_semi = false;
        } else if (accept(Token_while)) {
            DEFINE(cond, m_ast.push(logicOr()));
            DEFINE(body, m_ast.push(statement()));
            node = m_ast.make_while(cond, body);
            expect_semi = false;
        } else if (check(Token_brace_l)) {
            ASSIGN(node, block());
            expect_semi = false;
        } else {
            ASSIGN(node, assignment());

            if (node.id == Node_fn || node.id == Node_struct) {
                expect_semi = false;
            } else if (node.id == Node_id && accept(Token_colon)) {
                DEFINE(type, m_ast.push(expr()));
                DEFINE(value, accept(Token_eq) ? m_ast.push(tuple()) : n_none_ref);
                node = m_ast.make_var_decl(node.as.id.name, type, value);
            }
        }

        if (expect_semi) {
            EXPECT(Token_semi);
        }
        while (accept(Token_semi)) {
        }

        return node;
    }

    Node assignment() {
        DEFINE(node, tuple());

        if (accept(Token_eq)) {
            ASSIGN(node, m_ast.make_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_plus_eq)) {
            ASSIGN(node, m_ast.make_add_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_minus_eq)) {
            ASSIGN(node, m_ast.make_sub_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_aster_eq)) {
            ASSIGN(node, m_ast.make_mul_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_slash_eq)) {
            ASSIGN(node, m_ast.make_div_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_percent_eq)) {
            ASSIGN(node, m_ast.make_mod_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_less_dbl_eq)) {
            ASSIGN(node, m_ast.make_lsh_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_greater_dbl_eq)) {
            ASSIGN(node, m_ast.make_rsh_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_bar_eq)) {
            ASSIGN(node, m_ast.make_or_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_bar_dbl_eq)) {
            ASSIGN(node, m_ast.make_bitor_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_caret_eq)) {
            ASSIGN(node, m_ast.make_xor_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_amper_eq)) {
            ASSIGN(node, m_ast.make_and_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_amper_dbl_eq)) {
            ASSIGN(node, m_ast.make_bitand_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(Token_colon_eq)) {
            //@Incomplete Colon assignment lhs unchecked in parser, maybe change parsing to support
            // only the correct syntax?
            ASSIGN(node, m_ast.make_colon_assign(m_ast.push(node), m_ast.push(tuple())));
        }

        return node;
    }

    Node tuple() {
        bool trailing_comma_provided = false;
        DEFINE(nodes, sequence(&trailing_comma_provided));
        DEFER({ nodes.deinit(); })
        return (nodes.size > 1 || trailing_comma_provided) ? m_ast.make_tuple(nodes)
                                                           : nodes.front();
    }

    Node expr() {
        Node node;

        if (accept(Token_fn)) {
            Id name = 0;
            if (check(Token_id)) {
                ASSIGN(name, identifier());
            }

            EXPECT(Token_par_l);

            Array<NamedNode> params{};
            DEFER({ params.deinit(); })

            if (!accept(Token_par_r)) {
                do {
                    APPEND(params, declaration());
                } while (accept(Token_comma));

                EXPECT(Token_par_r);
            }

            DEFINE(ret_type, accept(Token_minus_greater) ? expr() : m_ast.make_void());

            Node body;
            bool has_body = false;
            if (check(Token_brace_l)) {
                has_body = true;
                ASSIGN(body, block());
            }

            assert(has_body && "function type/ptr parsing is not implemented");
            node = m_ast.make_fn(name, params, m_ast.push(ret_type), m_ast.push(body));
        } else if (accept(Token_struct)) {
            Id name = 0;
            if (check(Token_id)) {
                ASSIGN(name, identifier());
            }

            EXPECT(Token_brace_l);

            Array<NamedNode> fields{};
            DEFER({ fields.deinit(); })

            while (!accept(Token_brace_r)) {
                APPEND(fields, declaration());
                EXPECT(Token_semi);
            }

            node = m_ast.make_struct(name, fields.slice());
        } else {
            ASSIGN(node, ternary());
        }

        return node;
    }

    Node ternary() {
        DEFINE(node, logicOr());

        while (true) {
            if (accept(Token_question)) {
                DEFINE(then_body, logicOr());
                EXPECT(Token_colon);
                DEFINE(else_body, logicOr());
                node = m_ast.make_ternary(
                    m_ast.push(node), m_ast.push(then_body), m_ast.push(else_body));
            } else {
                break;
            }
        }

        return node;
    }

    Node logicOr() {
        DEFINE(node, logicAnd());

        while (true) {
            if (accept(Token_bar_dbl)) {
                ASSIGN(node, m_ast.make_or(m_ast.push(node), m_ast.push(logicOr())));
            } else {
                break;
            }
        }

        return node;
    }

    Node logicAnd() {
        DEFINE(node, bitOr());

        while (true) {
            if (accept(Token_amper_dbl)) {
                ASSIGN(node, m_ast.make_and(m_ast.push(node), m_ast.push(bitOr())));
            } else {
                break;
            }
        }

        return node;
    }

    Node bitOr() {
        DEFINE(node, bitXor());

        while (true) {
            if (accept(Token_bar)) {
                ASSIGN(node, m_ast.make_bitor(m_ast.push(node), m_ast.push(bitXor())));
            } else {
                break;
            }
        }

        return node;
    }

    Node bitXor() {
        DEFINE(node, bitAnd());

        while (true) {
            if (accept(Token_caret)) {
                ASSIGN(node, m_ast.make_xor(m_ast.push(node), m_ast.push(bitAnd())));
            } else {
                break;
            }
        }

        return node;
    }

    Node bitAnd() {
        DEFINE(node, equality());

        while (true) {
            if (accept(Token_amper)) {
                ASSIGN(node, m_ast.make_bitand(m_ast.push(node), m_ast.push(equality())));
            } else {
                break;
            }
        }

        return node;
    }

    Node equality() {
        DEFINE(node, relation());

        while (true) {
            if (accept(Token_eq_dbl)) {
                ASSIGN(node, m_ast.make_eq(m_ast.push(node), m_ast.push(relation())));
            } else if (accept(Token_exclam_eq)) {
                ASSIGN(node, m_ast.make_ne(m_ast.push(node), m_ast.push(relation())));
            } else {
                break;
            }
        }

        return node;
    }

    Node relation() {
        DEFINE(node, shift());

        while (true) {
            if (accept(Token_less)) {
                ASSIGN(node, m_ast.make_lt(m_ast.push(node), m_ast.push(shift())));
            } else if (accept(Token_less_eq)) {
                ASSIGN(node, m_ast.make_le(m_ast.push(node), m_ast.push(shift())));
            } else if (accept(Token_greater)) {
                ASSIGN(node, m_ast.make_gt(m_ast.push(node), m_ast.push(shift())));
            } else if (accept(Token_greater_eq)) {
                ASSIGN(node, m_ast.make_ge(m_ast.push(node), m_ast.push(shift())));
            } else {
                break;
            }
        }

        return node;
    }

    Node shift() {
        DEFINE(node, addSub());

        while (true) {
            if (accept(Token_less_dbl)) {
                ASSIGN(node, m_ast.make_lsh(m_ast.push(node), m_ast.push(addSub())));
            } else if (accept(Token_greater_dbl)) {
                ASSIGN(node, m_ast.make_rsh(m_ast.push(node), m_ast.push(addSub())));
            } else {
                break;
            }
        }

        return node;
    }

    Node addSub() {
        DEFINE(node, mulDiv());

        while (true) {
            if (accept(Token_plus)) {
                ASSIGN(node, m_ast.make_add(m_ast.push(node), m_ast.push(mulDiv())));
            } else if (accept(Token_minus)) {
                ASSIGN(node, m_ast.make_sub(m_ast.push(node), m_ast.push(mulDiv())));
            } else {
                break;
            }
        }

        return node;
    }

    Node mulDiv() {
        DEFINE(node, prefix());

        while (true) {
            if (accept(Token_aster)) {
                ASSIGN(node, m_ast.make_mul(m_ast.push(node), m_ast.push(prefix())));
            } else if (accept(Token_slash)) {
                ASSIGN(node, m_ast.make_div(m_ast.push(node), m_ast.push(prefix())));
            } else if (accept(Token_percent)) {
                ASSIGN(node, m_ast.make_mod(m_ast.push(node), m_ast.push(prefix())));
            } else {
                break;
            }
        }

        return node;
    }

    Node prefix() {
        Node node;

        if (accept(Token_plus)) {
            ASSIGN(node, m_ast.make_uplus(m_ast.push(suffix())));
        } else if (accept(Token_minus)) {
            ASSIGN(node, m_ast.make_uminus(m_ast.push(suffix())));
        } else if (accept(Token_exclam)) {
            ASSIGN(node, m_ast.make_not(m_ast.push(suffix())));
        } else if (accept(Token_tilde)) {
            ASSIGN(node, m_ast.make_compl(m_ast.push(suffix())));
        } else if (accept(Token_amper)) {
            ASSIGN(node, m_ast.make_addr(m_ast.push(prefix())));
        } else if (accept(Token_aster)) {
            ASSIGN(node, m_ast.make_deref(m_ast.push(prefix())));
        } else if (accept(Token_cast)) {
            EXPECT(Token_par_l);
            DEFINE(lhs, m_ast.push(expr()));
            EXPECT(Token_par_r);
            DEFINE(rhs, m_ast.push(prefix()));
            ASSIGN(node, m_ast.make_cast(lhs, rhs));
        } else {
            ASSIGN(node, suffix());
        }

        return node;
    }

    Node suffix() {
        DEFINE(node, term());

        while (true) {
            if (accept(Token_par_l)) {
                Array<Node> args{};
                DEFER({ args.deinit(); })
                if (!accept(Token_par_r)) {
                    ASSIGN(args, sequence());
                    EXPECT(Token_par_r);
                }
                node = m_ast.make_call(m_ast.push(node), args);
            } else if (accept(Token_bracket_l)) {
                ASSIGN(
                    node,
                    m_ast.make_index(
                        m_ast.push(node),
                        check(Token_bracket_r) ? n_none_ref : m_ast.push(tuple())));
                EXPECT(Token_bracket_r);
            } else if (accept(Token_period)) {
                if (!check(Token_id)) {
                    return error("identifier expected"), Node{};
                }
                DEFINE(name, identifier());
                node = m_ast.make_member(m_ast.push(node), name);
            } else {
                break;
            }
        }

        return node;
    }

    Node term() {
        Node node;

        if (check(Token_int_const)) {
            LOG_DBG("accept(int_const, \"\")", m_cur_token->text.size, m_cur_token->text.data)
            int64_t value = 0;
            int res = std::sscanf(m_cur_token->text.data, "%ld", &value);
            (void)res;
            assert(res > 0 && res != EOF && "integer constant parsing failed");
            node = m_ast.make_numeric_i64(value);
            getToken();
        } else if (check(Token_float_const)) {
            LOG_DBG("accept(float_const, \"\"", m_cur_token->text.size, m_cur_token->text.data)
            double value = 0.;
            //@Todo Replace std::sscanf!
            int res = std::sscanf(m_cur_token->text.data, "%lf", &value);
            (void)res;
            assert(res > 0 && res != EOF && "float constant parsing failed");
            node = m_ast.make_numeric_f64(value);
            getToken();
        } else if (check(Token_str_const)) {
            LOG_DBG("accept(str_const, \"\")", m_cur_token->text.size, m_cur_token->text.data)
            node = m_ast.make_string_literal(m_cur_token->text);
            getToken();
        }

        else if (check(Token_id)) {
            ASSIGN(node, m_ast.make_id(identifier()));
        }

        //@Todo Parse struct literal
        // else if (accept(Token_new)) {
        //     DEFINE(type, m_ast.push(expr()));

        //     Array<NamedNode> fields{};
        //     DEFER({ fields.deinit(); })
        //     if (accept(Token_brace_l) && !accept(Token_brace_r)) {
        //         do {
        //             APPEND(fields, declaration());
        //         } while (accept(Token_comma) && !check(Token_brace_r));

        //         EXPECT(Token_brace_r);
        //     }

        //     node = m_ast.make_struct_literal(type, fields);
        // }

        else if (accept(Token_u8)) {
            node = m_ast.make_u8();
        } else if (accept(Token_u16)) {
            node = m_ast.make_u16();
        } else if (accept(Token_u32)) {
            node = m_ast.make_u32();
        } else if (accept(Token_u64)) {
            node = m_ast.make_u64();
        } else if (accept(Token_i8)) {
            node = m_ast.make_i8();
        } else if (accept(Token_i16)) {
            node = m_ast.make_i16();
        } else if (accept(Token_i32)) {
            node = m_ast.make_i32();
        } else if (accept(Token_i64)) {
            node = m_ast.make_i64();
        } else if (accept(Token_f32)) {
            node = m_ast.make_f32();
        } else if (accept(Token_f64)) {
            node = m_ast.make_f64();
        } else if (accept(Token_void)) {
            node = m_ast.make_void();
        }

        else if (accept(Token_array_t)) {
            EXPECT(Token_par_l);
            DEFINE(type, m_ast.push(expr()));
            EXPECT(Token_comma);
            DEFINE(size, m_ast.push(expr()));
            EXPECT(Token_par_r);
            ASSIGN(node, m_ast.make_array_type(type, size));
        } else if (accept(Token_tuple_t)) {
            EXPECT(Token_par_l);
            DEFINE(nodes, sequence());
            DEFER({ nodes.deinit(); })
            EXPECT(Token_par_r);
            ASSIGN(node, m_ast.make_tuple_type(nodes));
        }

        else if (accept(Token_true)) {
            node = m_ast.make_numeric_i8(1);
        } else if (accept(Token_false)) {
            node = m_ast.make_numeric_i8(0);
        }

        else if (accept(Token_par_l)) {
            ASSIGN(node, assignment());
            EXPECT(Token_par_r);
        }

        else if (accept(Token_bracket_l)) {
            if (check(Token_bracket_r)) {
                ASSIGN(node, m_ast.make_array({}));
            } else {
                DEFINE(first, expr());
                Array<Node> nodes{};
                DEFER({ nodes.deinit(); })
                nodes.push() = first;
                while (accept(Token_comma)) {
                    if (check(Token_bracket_r)) {
                        break;
                    }
                    APPEND(nodes, expr());
                }
                ASSIGN(node, m_ast.make_array(std::move(nodes)));
            }
            EXPECT(Token_bracket_r);
        }

        else if (check(Token_eof)) {
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
        m_err = tmpstr_vformat(fmt, ap);
        va_end(ap);
    }
};

} // namespace

bool Parser::parse(TokenArray const &tokens) {
    assert(tokens.size && "empty token array");

    ast.init();

    root = n_none_ref;
    err = {};

    ParseEngine engine{tokens, ast, root, err};
    engine.parse();

    auto str = ast_inspect(root);
    LOG_INF("root: %.*s", str.size, str.data)

    return !engine.m_error_occurred;
}

} // namespace nkl
