#include "ntk/sys/term.h"

#include <io.h>

bool nk_isatty(i32 fd) {
    return _isatty(fd);
}
