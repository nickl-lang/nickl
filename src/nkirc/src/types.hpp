#ifndef HEADER_GUARD_NKB_TYPES
#define HEADER_GUARD_NKB_TYPES

#include <cstddef>

#include "irc.hpp"
#include "nkb/common.h"
#include "ntk/string.h"

nktype_t nkir_makeNumericType(NkIrCompiler c, NkIrNumericValueType value_type);
nktype_t nkir_makePointerType(NkIrCompiler c, nktype_t target_type);
nktype_t nkir_makeProcedureType(NkIrCompiler c, NkIrProcInfo proc_info);
nktype_t nkir_makeArrayType(NkIrCompiler c, nktype_t elem_t, size_t count);
nktype_t nkir_makeVoidType(NkIrCompiler c);
nktype_t nkir_makeAggregateType(NkIrCompiler c, nktype_t const *elem_types, size_t const *elem_counts, size_t n);

#endif // HEADER_GUARD_NKB_TYPES
