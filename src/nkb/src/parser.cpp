#include "parser.hpp"

#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(parser);

struct GeneratorState {
    NkSlice<NkIrToken> const m_tokens;
    NkAllocator m_tmp_alloc;

    nkstr m_error_msg{};
    bool m_ok{true};

    void generate() {
    }
};

} // namespace

void nkir_parse(NkIrParserState *parser, NkAllocator tmp_alloc, NkSlice<NkIrToken> tokens) {
    NK_LOG_TRC("%s", __func__);

    parser->error_msg = {};
    parser->ok = true;

    GeneratorState gen{
        .m_tokens = tokens,
        .m_tmp_alloc = tmp_alloc,
    };

    gen.generate();

    if (!gen.m_ok) {
        parser->error_msg = gen.m_error_msg;
        parser->ok = false;
    }
}
