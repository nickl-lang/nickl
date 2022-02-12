#include "nkl/core/parser.hpp"

#include <cassert>
#include <cstdio>

#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"

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

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::parser)

struct ParseEngine {
    TokenArray const &m_tokens;
    Ast &m_ast;
    node_ref_t &m_root;
    string &m_err;

    bool m_error_occurred = false;

    Token const *m_cur_token{};

    void parse() {
        EASY_FUNCTION(::profiler::colors::Green200);

        assert(m_tokens.size && (m_tokens.end() - 1)->id == t_eof && "ill-formed token stream");

        m_cur_token = m_tokens.begin();
        m_root = m_ast.push(block(false));
    }

private:
    void getToken() {
        assert(m_cur_token->id != t_eof);
        m_cur_token++;

        LOG_DBG("next token: " LOG_TOKEN(m_cur_token->id))
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

    Array<Node> sequence(bool *allow_trailing_comma = nullptr, bool *all_ids = nullptr) {
        Array<Node> nodes{};
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
        return nodes;
    }

    Node block(bool capture_brace = true) {
        //@Refactor Use tmp_allocator in parser for nodes
        Array<Node> nodes{};
        DEFER({ nodes.deinit(); })

        bool expect_brace_r = capture_brace && accept(t_brace_l);

        while (!check(t_eof) && (!expect_brace_r || !check(t_brace_r))) {
            LOG_DBG("next statement: token=" LOG_TOKEN(m_cur_token->id))
            APPEND(nodes, statement());
            LOG_DBG("end of statement")
        }

        if (expect_brace_r) {
            EXPECT(t_brace_r);
        }

        return nodes.size == 0   ? m_ast.make_nop()
               : nodes.size == 1 ? nodes.front()
                                 : m_ast.make_block(nodes.slice());
    }

    NamedNode declaration() {
        if (!check(t_id)) {
            return error("identifier expected"), NamedNode{};
        }
        NamedNode decl;
        ASSIGN(decl.name, identifier());
        EXPECT(t_colon);
        ASSIGN(decl.node, m_ast.push(expr()));
        return decl;
    }

    Id identifier() {
        LOG_DBG("accept(id, \"%.*s\")", m_cur_token->text.size, m_cur_token->text.data)
        Id id = s2id(m_cur_token->text);
        getToken();
        return id;
    }

    struct FnSignature {
        Id name;
        Array<NamedNode> params;
        Node ret_t;
    };

    FnSignature fnSignature(bool *accept_variadic = nullptr) {
        accept(t_fn);

        Id name = 0;
        if (check(t_id)) {
            ASSIGN(name, identifier());
        }

        EXPECT(t_par_l);

        Array<NamedNode> params{};

        if (!accept(t_par_r)) {
            do {
                if (accept_variadic && accept(t_period_3x)) {
                    *accept_variadic = true;
                    break;
                }
                APPEND(params, declaration());
            } while (accept(t_comma));

            EXPECT(t_par_r);
        }

        DEFINE(ret_t, accept(t_minus_greater) ? expr() : m_ast.make_void());

        return {name, params, ret_t};
    }

    Node statement() {
        Node node;

        bool expect_semi = true;

        if (accept(t_return)) {
            if (check(t_semi)) {
                node = m_ast.make_return(nullptr);
            } else {
                ASSIGN(node, m_ast.make_return(m_ast.push(tuple())));
            }
        } else if (accept(t_break)) {
            node = m_ast.make_break();
        } else if (accept(t_continue)) {
            node = m_ast.make_continue();
        } else if (accept(t_if)) {
            DEFINE(cond, m_ast.push(assignment()));
            DEFINE(then_body, m_ast.push(statement()));
            DEFINE(else_body, accept(t_else) ? m_ast.push(statement()) : n_none_ref);
            node = m_ast.make_if(cond, then_body, else_body);
            expect_semi = false;
        } else if (accept(t_while)) {
            DEFINE(cond, m_ast.push(assignment()));
            DEFINE(body, m_ast.push(statement()));
            node = m_ast.make_while(cond, body);
            expect_semi = false;
        } else if (check(t_brace_l)) {
            ASSIGN(node, block());
            expect_semi = false;
        } else if (accept(t_foreign)) {
            //@Todo Handle escaped strings in foreign library names
            if (!check(t_str_const)) {
                return error("foreign library name expected"), Node{};
            }
            Id lib = s2id(m_cur_token->text);
            getToken();
            bool is_variadic = false;
            DEFINE(sig, fnSignature(&is_variadic));
            DEFER({ sig.params.deinit(); });
            node = m_ast.make_foreign_fn(
                lib, sig.name, sig.params.slice(), m_ast.push(sig.ret_t), is_variadic);
        } else {
            ASSIGN(node, assignment());

            if (node.id == Node_fn || node.id == Node_struct) {
                expect_semi = false;
            } else if (node.id == Node_id && accept(t_colon)) {
                DEFINE(type, m_ast.push(expr()));
                DEFINE(value, accept(t_eq) ? m_ast.push(tuple()) : n_none_ref);
                node = m_ast.make_var_decl(node.as.id.name, type, value);
            } else if ((node.id == Node_id || node.id == Node_id_tuple)) {
                if (accept(t_colon)) {
                    DEFINE(type, m_ast.push(expr()));
                    DEFINE(value, accept(t_eq) ? m_ast.push(tuple()) : n_none_ref);
                    node = m_ast.make_var_decl(node.as.id.name, type, value);
                } else if (accept(t_colon_eq)) {
                    ASSIGN(node, m_ast.make_colon_assign(m_ast.push(node), m_ast.push(tuple())));
                }
            }
        }

        if (expect_semi) {
            EXPECT(t_semi);
        }
        while (accept(t_semi)) {
        }

        return node;
    }

    Node assignment() {
        DEFINE(node, tuple());

        if (accept(t_eq)) {
            ASSIGN(node, m_ast.make_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_plus_eq)) {
            ASSIGN(node, m_ast.make_add_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_minus_eq)) {
            ASSIGN(node, m_ast.make_sub_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_aster_eq)) {
            ASSIGN(node, m_ast.make_mul_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_slash_eq)) {
            ASSIGN(node, m_ast.make_div_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_percent_eq)) {
            ASSIGN(node, m_ast.make_mod_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_less_2x_eq)) {
            ASSIGN(node, m_ast.make_lsh_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_greater_2x_eq)) {
            ASSIGN(node, m_ast.make_rsh_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_bar_eq)) {
            ASSIGN(node, m_ast.make_or_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_bar_2x_eq)) {
            ASSIGN(node, m_ast.make_bitor_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_caret_eq)) {
            ASSIGN(node, m_ast.make_xor_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_amper_eq)) {
            ASSIGN(node, m_ast.make_and_assign(m_ast.push(node), m_ast.push(tuple())));
        } else if (accept(t_amper_2x_eq)) {
            ASSIGN(node, m_ast.make_bitand_assign(m_ast.push(node), m_ast.push(tuple())));
        }

        return node;
    }

    Node tuple() {
        bool trailing_comma_provided = false;
        bool all_ids = false;
        DEFINE(nodes, sequence(&trailing_comma_provided, &all_ids));
        DEFER({ nodes.deinit(); })
        return (nodes.size > 1 || trailing_comma_provided)
                   ? (all_ids ? m_ast.make_id_tuple(nodes) : m_ast.make_tuple(nodes))
                   : nodes.front();
    }

    Node expr() {
        Node node;

        if (accept(t_fn)) {
            auto sig = fnSignature();
            DEFER({ sig.params.deinit(); });

            Node body;
            bool has_body = false;
            if (check(t_brace_l)) {
                has_body = true;
                ASSIGN(body, block());
            }

            assert(has_body && "function type/ptr parsing is not implemented");
            node = m_ast.make_fn(sig.name, sig.params, m_ast.push(sig.ret_t), m_ast.push(body));
        } else if (accept(t_struct)) {
            Id name = 0;
            if (check(t_id)) {
                ASSIGN(name, identifier());
            }

            EXPECT(t_brace_l);

            Array<NamedNode> fields{};
            DEFER({ fields.deinit(); })

            while (!accept(t_brace_r)) {
                APPEND(fields, declaration());
                EXPECT(t_semi);
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
            if (accept(t_question)) {
                DEFINE(then_body, logicOr());
                EXPECT(t_colon);
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
            if (accept(t_bar_2x)) {
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
            if (accept(t_amper_2x)) {
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
            if (accept(t_bar)) {
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
            if (accept(t_caret)) {
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
            if (accept(t_amper)) {
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
            if (accept(t_eq_2x)) {
                ASSIGN(node, m_ast.make_eq(m_ast.push(node), m_ast.push(relation())));
            } else if (accept(t_exclam_eq)) {
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
            if (accept(t_less)) {
                ASSIGN(node, m_ast.make_lt(m_ast.push(node), m_ast.push(shift())));
            } else if (accept(t_less_eq)) {
                ASSIGN(node, m_ast.make_le(m_ast.push(node), m_ast.push(shift())));
            } else if (accept(t_greater)) {
                ASSIGN(node, m_ast.make_gt(m_ast.push(node), m_ast.push(shift())));
            } else if (accept(t_greater_eq)) {
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
            if (accept(t_less_2x)) {
                ASSIGN(node, m_ast.make_lsh(m_ast.push(node), m_ast.push(addSub())));
            } else if (accept(t_greater_2x)) {
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
            if (accept(t_plus)) {
                ASSIGN(node, m_ast.make_add(m_ast.push(node), m_ast.push(mulDiv())));
            } else if (accept(t_minus)) {
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
            if (accept(t_aster)) {
                ASSIGN(node, m_ast.make_mul(m_ast.push(node), m_ast.push(prefix())));
            } else if (accept(t_slash)) {
                ASSIGN(node, m_ast.make_div(m_ast.push(node), m_ast.push(prefix())));
            } else if (accept(t_percent)) {
                ASSIGN(node, m_ast.make_mod(m_ast.push(node), m_ast.push(prefix())));
            } else {
                break;
            }
        }

        return node;
    }

    Node prefix() {
        Node node;

        if (accept(t_plus)) {
            ASSIGN(node, m_ast.make_uplus(m_ast.push(suffix())));
        } else if (accept(t_minus)) {
            ASSIGN(node, m_ast.make_uminus(m_ast.push(suffix())));
        } else if (accept(t_exclam)) {
            ASSIGN(node, m_ast.make_not(m_ast.push(suffix())));
        } else if (accept(t_tilde)) {
            ASSIGN(node, m_ast.make_compl(m_ast.push(suffix())));
        } else if (accept(t_amper)) {
            ASSIGN(node, m_ast.make_addr(m_ast.push(prefix())));
        } else if (accept(t_aster)) {
            ASSIGN(node, m_ast.make_deref(m_ast.push(prefix())));
        } else if (accept(t_cast)) {
            EXPECT(t_par_l);
            DEFINE(lhs, m_ast.push(expr()));
            EXPECT(t_par_r);
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
            if (accept(t_par_l)) {
                Array<Node> args{};
                DEFER({ args.deinit(); })
                if (!accept(t_par_r)) {
                    ASSIGN(args, sequence());
                    EXPECT(t_par_r);
                }
                node = m_ast.make_call(m_ast.push(node), args);
            } else if (accept(t_bracket_l)) {
                ASSIGN(
                    node,
                    m_ast.make_index(
                        m_ast.push(node), check(t_bracket_r) ? n_none_ref : m_ast.push(tuple())));
                EXPECT(t_bracket_r);
            } else if (accept(t_period)) {
                if (!check(t_id) && !check(t_int_const)) {
                    return error("identifier or integer expected"), Node{};
                }
                if (check(t_id)) {
                    DEFINE(name, identifier());
                    node = m_ast.make_member(m_ast.push(node), name);
                } else {
                    DEFINE(index, m_ast.push(term()));
                    node = m_ast.make_tuple_index(m_ast.push(node), index);
                }

            } else {
                break;
            }
        }

        return node;
    }

    Node term() {
        Node node;

        if (check(t_int_const)) {
            LOG_DBG("accept(int_const, \"\")", m_cur_token->text.size, m_cur_token->text.data)
            int64_t value = 0;
            int res = std::sscanf(m_cur_token->text.data, "%ld", &value);
            (void)res;
            assert(res > 0 && res != EOF && "integer constant parsing failed");
            node = m_ast.make_numeric_i64(value);
            getToken();
        } else if (check(t_float_const)) {
            LOG_DBG("accept(float_const, \"\"", m_cur_token->text.size, m_cur_token->text.data)
            double value = 0.;
            //@Todo Replace std::sscanf!
            int res = std::sscanf(m_cur_token->text.data, "%lf", &value);
            (void)res;
            assert(res > 0 && res != EOF && "float constant parsing failed");
            node = m_ast.make_numeric_f64(value);
            getToken();
        } else if (check(t_str_const)) {
            LOG_DBG("accept(str_const, \"\")", m_cur_token->text.size, m_cur_token->text.data)
            node = m_ast.make_string_literal(m_cur_token->text);
            getToken();
        } else if (check(t_escaped_str_const)) {
            //@Todo Implement escaped str consts parsing
            LOG_DBG(
                "accept(escaped_str_const, \"\")", m_cur_token->text.size, m_cur_token->text.data)
            node = m_ast.make_string_literal(m_cur_token->text);
            getToken();
        }

        else if (check(t_id)) {
            ASSIGN(node, m_ast.make_id(identifier()));
        }

        //@Todo Parse struct literal
        // else if (accept(t_new)) {
        //     DEFINE(type, m_ast.push(expr()));

        //     Array<NamedNode> fields{};
        //     DEFER({ fields.deinit(); })
        //     if (accept(t_brace_l) && !accept(t_brace_r)) {
        //         do {
        //             APPEND(fields, declaration());
        //         } while (accept(t_comma) && !check(t_brace_r));

        //         EXPECT(t_brace_r);
        //     }

        //     node = m_ast.make_struct_literal(type, fields);
        // }

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

        else if (accept(t_void)) {
            node = m_ast.make_void();
        } else if (accept(t_bool)) {
            node = m_ast.make_i8();
        } else if (accept(t_float)) {
            node = m_ast.make_f64();
        } else if (accept(t_int)) {
            node = m_ast.make_i64();
        } else if (accept(t_uint)) {
            node = m_ast.make_i64();
        } else if (accept(t_string)) {
            Node types[] = {{}, {}};
            node = m_ast.make_tuple_type({types, sizeof(types) / sizeof(types[0])});
        }

        else if (accept(t_array_t)) {
            EXPECT(t_brace_l);
            DEFINE(type, m_ast.push(expr()));
            EXPECT(t_comma);
            DEFINE(size, m_ast.push(expr()));
            EXPECT(t_brace_r);
            ASSIGN(node, m_ast.make_array_type(type, size));
        } else if (accept(t_tuple_t)) {
            EXPECT(t_brace_l);
            DEFINE(nodes, sequence());
            DEFER({ nodes.deinit(); })
            EXPECT(t_brace_r);
            ASSIGN(node, m_ast.make_tuple_type(nodes));
        } else if (accept(t_ptr_t)) {
            EXPECT(t_brace_l);
            DEFINE(target, m_ast.push(expr()));
            EXPECT(t_brace_r);
            ASSIGN(node, m_ast.make_ptr_type(target));
        }

        else if (accept(t_true)) {
            node = m_ast.make_numeric_i8(1);
        } else if (accept(t_false)) {
            node = m_ast.make_numeric_i8(0);
        }

        else if (accept(t_par_l)) {
            ASSIGN(node, assignment());
            EXPECT(t_par_r);
        }

        else if (accept(t_bracket_l)) {
            if (check(t_bracket_r)) {
                ASSIGN(node, m_ast.make_array({}));
            } else {
                DEFINE(first, expr());
                Array<Node> nodes{};
                DEFER({ nodes.deinit(); })
                nodes.push() = first;
                while (accept(t_comma)) {
                    if (check(t_bracket_r)) {
                        break;
                    }
                    APPEND(nodes, expr());
                }
                ASSIGN(node, m_ast.make_array(std::move(nodes)));
            }
            EXPECT(t_bracket_r);
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
