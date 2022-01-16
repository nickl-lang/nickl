#include "nkl/core/core.hpp"

#include "nk/common/logger.hpp"

LOG_USE_SCOPE(core)

void lang_init() {
    LOG_TRC("lang_init")
}

void lang_deinit() {
    LOG_TRC("lang_deinit")
}

int lang_runFile(char const *filename) {
    LOG_TRC("lang_runFile(filename=%s)", filename)

    return 0;
}
