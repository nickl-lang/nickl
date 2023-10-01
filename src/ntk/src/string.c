#include "ntk/string.h"

extern inline nkstr nk_strcpy(NkAllocator alloc, nkstr src);
extern inline nkstr nk_mkstr(char const *str);
extern inline nkstr nks_trim_left(nkstr str);
extern inline nkstr nks_trim_right(nkstr str);
extern inline nkstr nks_trim(nkstr str);
extern inline nkstr nks_chop_by_delim(nkstr *str, char delim);
