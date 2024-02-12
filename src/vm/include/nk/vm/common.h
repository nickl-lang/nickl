#ifndef NK_VM_COMMON_H_
#define NK_VM_COMMON_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkType const *nktype_t;

typedef struct {
    void *data;
    nktype_t type;
} nkval_t;

// TODO Move somewhere
#define DEFINE_ID_TYPE(NAME) \
    typedef struct {         \
        usize id;            \
    } NAME

#ifdef __cplusplus
}
#endif

#endif // NK_VM_COMMON_H_
