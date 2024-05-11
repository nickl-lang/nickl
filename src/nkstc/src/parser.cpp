#include "parser.h"

#include "lexer.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(parser);

struct Void {};

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_nkst_token_id[ID], s_nkst_token_text[ID]

#define CHECK(EXPR)            \
    EXPR;                      \
    if (nkl_getErrorCount()) { \
        return {};             \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(nkda_append((AR), (VAL)))
#define EXPECT(ID) CHECK(expect(ID))

struct ParseEngine {
    NkAtom const m_file;
    NkString const m_text;
    NklTokenArray const m_tokens;

    NklAstNodeDynArray *m_nodes;

    u32 m_cur_token_idx{};

    Void parse() {
        nk_assert(m_tokens.size && nk_slice_last(m_tokens).id == t_eof && "ill-formed token stream");

        nkda_append(m_nodes, NklAstNode{0, m_cur_token_idx, 0, 0});
        auto &node = nk_slice_last(*m_nodes);
        node.id = nk_cs2atom("list");
        node.total_children = m_nodes->size;
        CHECK(parseNodeList(node));

        if (!check(t_eof)) {
            auto const token_str = nkl_getTokenStr(curToken(), m_text);
            return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
        }

        return {};
    }

    Void parseNode() {
        nkda_append(m_nodes, NklAstNode{0, m_cur_token_idx, 0, 0});
        auto &node = nk_slice_last(*m_nodes);

        if (accept(t_par_l)) {
            if (!accept(t_par_r)) {
                if (check(t_id)) {
                    auto const token_str = nkl_getTokenStr(curToken(), m_text);
                    node.id = nk_s2atom(token_str);
                    getToken();
                } else if (check(t_string)) {
                    // TODO: Use scratch arena
                    auto str = parseString(nk_default_allocator);
                    node.id = nk_s2atom(str);
                    nk_free(nk_default_allocator, (void *)str.data, str.size + 1);
                } else {
                    auto const token_str = nkl_getTokenStr(curToken(), m_text);
                    return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
                }
                node.token_idx++;
                node.total_children = m_nodes->size;
                CHECK(parseNodeList(node));
                EXPECT(t_par_r);
            }
        } else if (accept(t_bracket_l)) {
            node.id = nk_cs2atom("list");
            node.total_children = m_nodes->size;
            CHECK(parseNodeList(node));
            EXPECT(t_bracket_r);
        }

        else if (accept(t_id)) {
            node.id = nk_cs2atom("id");
        }

        else if (accept(t_int)) {
            node.id = nk_cs2atom("int");
        } else if (check(t_float)) {
            node.id = nk_cs2atom("float");
        }

        else if (accept(t_string)) {
            node.id = nk_cs2atom("string");
        } else if (accept(t_escaped_string)) {
            node.id = nk_cs2atom("escaped_string");
        }

        else {
            auto const token_str = nkl_getTokenStr(curToken(), m_text);
            return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
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

    NkString parseString(NkAllocator alloc) {
        auto const data = m_text.data + curToken()->pos + 1;
        auto const len = curToken()->len - 2;
        getToken();

        return nks_copyNt(alloc, {data, len});
    }

    void getToken() {
        nk_assert(curToken()->id != t_eof);
        m_cur_token_idx++;
        NK_LOG_DBG("next token: " LOG_TOKEN(curToken()->id));
    }

    bool accept(NkStTokenId id) {
        if (check(id)) {
            NK_LOG_DBG("accept" LOG_TOKEN(id));
            getToken();
            return true;
        }
        return false;
    }

    bool check(NkStTokenId id) const {
        return curToken()->id == id;
    }

    void expect(NkStTokenId id) {
        if (!accept(id)) {
            auto const token_str = nkl_getTokenStr(curToken(), m_text);
            return error("expected `%s` before `" NKS_FMT "`", s_nkst_token_text[id], NKS_ARG(token_str));
        }
    }

    NklToken const *curToken() const {
        return &m_tokens.data[m_cur_token_idx];
    }

    NK_PRINTF_LIKE(2, 3) void error(char const *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        nkl_vreportError(m_file, curToken(), fmt, ap);
        va_end(ap);
    }
};

} // namespace

NklAstNodeArray nkst_parse(NkAllocator alloc, NkAtom file, NkString text, NklTokenArray tokens) {
    NK_LOG_TRC("%s", __func__);

    NklAstNodeDynArray nodes{NKDA_INIT(alloc)};
    nkda_reserve(&nodes, 1000);

    ParseEngine engine{
        .m_file = file,
        .m_text = text,
        .m_tokens = tokens,
        .m_nodes = &nodes,
    };

    engine.parse();

    NK_LOG_INF(
        "ast:%s", (char const *)[&]() {
            // TODO: Use scratch arena
            NkStringBuilder sb{NKSB_INIT(nk_default_allocator)};
            nkl_ast_inspect(
                {
                    .file = file,
                    .text = text,
                    .tokens = tokens,
                    .nodes = {NK_SLICE_INIT(nodes)},
                },
                nksb_getStream(&sb));
            nksb_appendNull(&sb);
            return nk_defer((char const *)sb.data, [sb]() mutable {
                nksb_free(&sb);
            });
        }());

    return {NK_SLICE_INIT(nodes)};
}
