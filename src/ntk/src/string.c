#include "ntk/string.h"

extern inline nks nk_strcpy(NkAllocator alloc, nks src);
extern inline nks nk_cs2s(char const *str);
extern inline nks nks_trim_left(nks str);
extern inline nks nks_trim_right(nks str);
extern inline nks nks_trim(nks str);
extern inline nks nks_chop_by_delim(nks *str, char delim);
extern inline nks nks_chop_by_delim_reverse(nks *str, char delim);
