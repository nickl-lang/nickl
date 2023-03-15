#ifndef HEADER_GUARD_NK_VM_COMMON
#define HEADER_GUARD_NK_VM_COMMON

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkType const *nktype_t;

typedef uint8_t nk_typeclassid_t;

typedef struct {
    void *data;
    nktype_t type;
} nkval_t;

// TODO Move somewhere
#define DEFINE_ID_TYPE(NAME) \
    typedef struct {         \
        size_t id;           \
    } NAME

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_COMMON
