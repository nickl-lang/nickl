#ifndef HEADER_GUARD_NK_SYS_TTY
#define HEADER_GUARD_NK_SYS_TTY

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NK_TERM_COLOR_NONE "\x1b[0m"
#define NK_TERM_COLOR_GRAY "\x1b[1;30m"
#define NK_TERM_COLOR_RED "\x1b[1;31m"
#define NK_TERM_COLOR_GREEN "\x1b[1;32m"
#define NK_TERM_COLOR_YELLOW "\x1b[1;33m"
#define NK_TERM_COLOR_BLUE "\x1b[1;34m"
#define NK_TERM_COLOR_MAGENTA "\x1b[1;35m"
#define NK_TERM_COLOR_CYAN "\x1b[1;36m"
#define NK_TERM_COLOR_WHITE "\x1b[1;37m"

bool nk_isatty();

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_TTY
