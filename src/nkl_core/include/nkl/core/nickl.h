#ifndef NKL_CORE_NICKL_H_
#define NKL_CORE_NICKL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
} NklState;

void nkl_state_init(NklState *nkl);
void nkl_state_free(NklState *nkl);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
