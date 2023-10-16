#ifndef HEADER_GUARD_NTK_CLI
#define HEADER_GUARD_NTK_CLI

#include "ntk/string.h"

#define NK_CLI_ARG_INIT(pargv, pkey, pval)                        \
    char const *const **_nk_cli_pargv = (pargv);                  \
    nks *_nk_cli_pkey = (pkey);                                   \
    nks *_nk_cli_pval = (pval);                                   \
    do {                                                          \
        nks _str = nk_cs2s(*(*_nk_cli_pargv)++);                  \
        if (nks_starts_with(_str, nk_cs2s("--"))) {               \
            *_nk_cli_pval = _str;                                 \
            *_nk_cli_pkey = nks_chop_by_delim(_nk_cli_pval, '='); \
        } else if (nks_first(_str) == '-') {                      \
            *_nk_cli_pkey = {_str.data, minu(_str.size, 2)};      \
            if (_str.size > 2) {                                  \
                *_nk_cli_pval = {_str.data + 2, _str.size - 2};   \
                if (nks_first(*_nk_cli_pval) == '=') {            \
                    _nk_cli_pval->data += 1;                      \
                    _nk_cli_pval->size -= 1;                      \
                }                                                 \
            }                                                     \
        } else {                                                  \
            *_nk_cli_pval = _str;                                 \
        }                                                         \
    } while (0)

#define NK_CLI_ARG_GET_VALUE                                                             \
    do {                                                                                 \
        if (!_nk_cli_pval->size && *(*_nk_cli_pargv) && (*(*_nk_cli_pargv))[0] != '-') { \
            *_nk_cli_pval = nk_cs2s(*(*_nk_cli_pargv)++);                                \
        }                                                                                \
    } while (0)

#endif // HEADER_GUARD_NTK_CLI
