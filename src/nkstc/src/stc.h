#ifndef NKSTC_STC_H_
#define NKSTC_STC_H_

#include "nkb/ir.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

int nkst_compile(NkString in_file, NkIrCompilerConfig conf);

int nkst_run(NkString in_file);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_STC_H_
