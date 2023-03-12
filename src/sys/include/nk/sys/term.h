#ifndef HEADER_GUARD_NK_SYS_TTY
#define HEADER_GUARD_NK_SYS_TTY

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TERM_COLOR_NONE "\x1b[0m"
#define TERM_COLOR_GRAY "\x1b[1;30m"
#define TERM_COLOR_RED "\x1b[1;31m"
#define TERM_COLOR_GREEN "\x1b[1;32m"
#define TERM_COLOR_YELLOW "\x1b[1;33m"
#define TERM_COLOR_BLUE "\x1b[1;34m"
#define TERM_COLOR_MAGENTA "\x1b[1;35m"
#define TERM_COLOR_CYAN "\x1b[1;36m"
#define TERM_COLOR_WHITE "\x1b[1;37m"

bool nksys_isatty();

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_TTY
