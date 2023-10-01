#include "ntk/sys/term.h"

#include <unistd.h>

bool nk_isatty(int fileno) {
    return isatty(fileno);
}
