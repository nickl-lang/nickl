#include "ntk/os/time.h"

#include <unistd.h>

void nk_usleep(u64 usec) {
    usleep(usec);
}
