#include "ntk/sys/term.h"

#include <io.h>

bool nk_isatty(int fd) {
    return _isatty(fd);
}
