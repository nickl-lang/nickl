#ifndef HEADER_GUARD_NK_VM_COMMON
#define HEADER_GUARD_NK_VM_COMMON

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkType const *nk_type_t;

typedef size_t nk_typeid_t;
typedef uint8_t nk_typeclassid_t;

typedef struct {
    void *data;
    nk_type_t type;
} nk_value_t;

typedef void (*FuncPtr)(nk_type_t self, nk_value_t ret, nk_value_t args);

// TODO Move somewhere
#define DEFINE_ID_TYPE(NAME) \
    typedef struct {         \
        size_t id;           \
    } NAME

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_COMMON
