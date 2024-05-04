#ifndef NKSTC_PARSER_H_
#define NKSTC_PARSER_H_

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

NklAstNodeArray nkst_parse(NklState nkl, NkAllocator alloc, NkString text, NklTokenArray tokens);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_PARSER_H_
