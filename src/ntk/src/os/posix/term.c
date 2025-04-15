#include "ntk/term.h"

#include <unistd.h>

bool nk_isatty(i32 fd) {
    return isatty(fd);
}
