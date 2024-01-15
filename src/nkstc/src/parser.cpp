#include "parser.h"

#include <cstdint>

#include "lexer.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "ntk/array.h"
#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(parser);

struct Void {};

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_nkst_token_id[ID], s_nkst_token_text[ID]

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(nkar_append((AR), (VAL)))
#define EXPECT(ID) CHECK(expect(ID))

struct ParseEngine {
    nks m_text;
    NklTokenView m_tokens;
    NklAstNodeArray *m_nodes;

    NkAllocator m_tmp_alloc;

    nks m_error_msg{};
    bool m_error_occurred{};

    uint32_t m_cur_token_idx{};

    Void parse() {
        assert(m_tokens.size && nkav_last(m_tokens).id == t_eof && "ill-formed token stream");

        nkar_append(m_nodes, NklAstNode{0, m_cur_token_idx, 0, 0});
        auto &node = nkav_last(*m_nodes);
        node.id = cs2nkid("list");
        node.total_children = m_nodes->size;
        CHECK(parseNodeList(node));

        if (!check(t_eof)) {
            auto const token_str = nkl_getTokenStr(*curToken(), m_text);
            return error("unexpected token `" nks_Fmt "`", nks_Arg(token_str)), Void{};
        }

        return {};
    }

    Void parseNode() {
        nkar_append(m_nodes, NklAstNode{0, m_cur_token_idx, 0, 0});
        auto &node = nkav_last(*m_nodes);

        if (accept(t_par_l)) {
            if (!accept(t_par_r)) {
                if (check(t_id)) {
                    auto const token_str = nkl_getTokenStr(*curToken(), m_text);
                    node.id = s2nkid(token_str);
                    getToken();
                } else if (check(t_string)) {
                    auto str = parseString(m_tmp_alloc);
                    node.id = s2nkid(str);
                } else {
                    auto const token_str = nkl_getTokenStr(*curToken(), m_text);
                    return error("unexpected token `" nks_Fmt "`", nks_Arg(token_str)), Void{};
                }
                node.token_idx++;
                node.total_children = m_nodes->size;
                CHECK(parseNodeList(node));
                EXPECT(t_par_r);
            }
        } else if (accept(t_bracket_l)) {
            node.id = cs2nkid("list");
            node.total_children = m_nodes->size;
            CHECK(parseNodeList(node));
            EXPECT(t_bracket_r);
        }

        else if (accept(t_id)) {
            node.id = cs2nkid("id");
        }

        else if (accept(t_int)) {
            node.id = cs2nkid("int");
        } else if (check(t_float)) {
            node.id = cs2nkid("float");
        }

        else if (accept(t_string) || accept(t_escaped_string)) {
            node.id = cs2nkid("string");
        }

        else {
            auto const token_str = nkl_getTokenStr(*curToken(), m_text);
            return error("unexpected token `" nks_Fmt "`", nks_Arg(token_str)), Void{};
        }

        return {};
    }

    Void parseNodeList(NklAstNode &node) {
        while (!check(t_eof) && !check(t_bracket_r) && !check(t_par_r)) {
            CHECK(parseNode());
            node.arity++;
        }
        node.total_children = m_nodes->size - node.total_children;
        return {};
    }

    nks parseString(NkAllocator alloc) {
        auto const data = m_text.data + curToken()->pos + 1;
        auto const len = curToken()->len - 2;
        getToken();

        return nk_strcpy_nt(alloc, {data, len});
    }

    void getToken() {
        assert(curToken()->id != t_eof);
        m_cur_token_idx++;
        NK_LOG_DBG("next token: " LOG_TOKEN(curToken()->id));
    }

    bool accept(ENkStTokenId id) {
        if (check(id)) {
            NK_LOG_DBG("accept" LOG_TOKEN(id));
            getToken();
            return true;
        }
        return false;
    }

    bool check(ENkStTokenId id) const {
        return curToken()->id == id;
    }

    void expect(ENkStTokenId id) {
        if (!accept(id)) {
            auto const token_str = nkl_getTokenStr(*curToken(), m_text);
            return error("expected `%s` before `" nks_Fmt "`", s_nkst_token_text[id], nks_Arg(token_str));
        }
    }

    NklToken const *curToken() const {
        return &m_tokens.data[m_cur_token_idx];
    }

    NK_PRINTF_LIKE(2, 3) void error(char const *fmt, ...) {
        assert(!m_error_occurred && "parser error is already initialized");

        va_list ap;
        va_start(ap, fmt);
        NkStringBuilder sb{};
        sb.alloc = m_tmp_alloc;
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = {nkav_init(sb)};
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

bool nkst_parse(NkStParserState *parser, NkArena *file_arena, NkArena *tmp_arena, nks text, NklTokenView tokens) {
    NK_LOG_TRC("%s", __func__);

    parser->nodes = {0, 0, 0, nk_arena_getAllocator(file_arena)};
    parser->error_msg = {};
    parser->error_token = {};

    ParseEngine engine{
        .m_text = text,
        .m_tokens = tokens,
        .m_nodes = &parser->nodes,
        .m_tmp_alloc{nk_arena_getAllocator(tmp_arena)},
    };

    engine.parse();

    NK_LOG_INF(
        "ast:%s", (char const *)[&]() {
            NkStringBuilder sb{0, 0, 0, engine.m_tmp_alloc};
            nkl_ast_inspect(
                {
                    text,
                    tokens,
                    {nkav_init(parser->nodes)},
                },
                nksb_getStream(&sb));
            nksb_append_null(&sb);
            return makeDeferrerWithData((char const *)sb.data, [sb]() mutable {
                nksb_free(&sb);
            });
        }());

    if (engine.m_error_occurred) {
        parser->error_msg = engine.m_error_msg;
        parser->error_token = tokens.data[engine.m_cur_token_idx];
        return false;
    }

    return true;
}
