#include "ntk/sys/term.h"

#include <unistd.h>

bool nk_isatty(int fd) {
    return isatty(fd);
}
