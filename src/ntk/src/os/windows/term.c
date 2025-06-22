#include "ntk/term.h"

#include "common.h"

bool nk_isatty(i32 fd) {
    HANDLE h = 0;
    switch (fd) {
        case 0:
            h = GetStdHandle(STD_INPUT_HANDLE);
            break;
        case 1:
            h = GetStdHandle(STD_OUTPUT_HANDLE);
            break;
        case 2:
            h = GetStdHandle(STD_ERROR_HANDLE);
            break;
        default:
            return false;
    }
    DWORD type = GetFileType(h);
    return type == FILE_TYPE_CHAR;
}
