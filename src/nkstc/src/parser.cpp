#include "parser.h"

#include "lexer.h"
#include "nkl/common/ast.h"
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
    NklTokenView const m_tokens;

    NkAllocator m_tmp_alloc;

    nks m_error_msg{};
    bool m_error_occurred{};

    NklToken *m_cur_token{};

    Void parse() {
        assert(m_tokens.size && nkav_last(m_tokens).id == t_eof && "ill-formed token stream");
        m_cur_token = &m_tokens.data[0];

        // TODO Hardcoded example {
        NklAstNode nodes[] = {
            {.id = cs2nkid("add"), .token_idx = 1, .total_children = 2, .arity = 2},
            {.id = cs2nkid("int"), .token_idx = 0, .total_children = 0, .arity = 0},
            {.id = cs2nkid("int"), .token_idx = 2, .total_children = 0, .arity = 0},
        };
        NklToken example_tokens[] = {
            {.text = nk_cs2s("4"), .id = 0, .lin = 1, .col = 1},
            {.text = nk_cs2s("+"), .id = 0, .lin = 1, .col = 2},
            {.text = nk_cs2s("5"), .id = 0, .lin = 1, .col = 3},
        };
        nk_stream out = nk_file_getStream(nk_stdout());
        nkl_ast_inspect({nodes, sizeof(nodes)}, {example_tokens, sizeof(example_tokens)}, out);
        // }

        return {};
    }

    void getToken() {
        assert(m_cur_token->id != t_eof);
        m_cur_token++;
        NK_LOG_DBG("next token: " LOG_TOKEN(m_cur_token->id));
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
        return m_cur_token->id == id;
    }

    void expect(ENkStTokenId id) {
        if (!accept(id)) {
            return error("expected `%s` before `" nks_Fmt "`", s_nkst_token_text[id], nks_Arg(m_cur_token->text));
        }
    }

    NK_PRINTF_LIKE(2, 3) void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Parser error already initialized");

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

bool nkst_parse(NkStParserState *parser, NkArena *file_arena, NkArena *tmp_arena, NklTokenView tokens) {
    NK_LOG_TRC("%s", __func__);

    parser->error_msg = {};
    parser->error_token = {};

    ParseEngine engine{
        .m_tokens = tokens,
        .m_tmp_alloc{nk_arena_getAllocator(tmp_arena)},
    };

    engine.parse();

    if (engine.m_error_occurred) {
        parser->error_msg = engine.m_error_msg;
        parser->error_token = *engine.m_cur_token;
        return false;
    }

    return true;
}
