#ifndef HEADER_GUARD_NK_COMMON_STRING_H
#define HEADER_GUARD_NK_COMMON_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char const *data;
    size_t size;
} nkstr;

nkstr cs2s(char const *str);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_H
