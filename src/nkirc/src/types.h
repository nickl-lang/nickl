#ifndef NKIRC_TYPES_H_
#define NKIRC_TYPES_H_

#include "irc.h"
#include "nkb/common.h"

#ifdef __cplusplus
extern "C" {
#endif

nktype_t nkir_makeNumericType(NkIrCompiler c, NkIrNumericValueType value_type);
nktype_t nkir_makePointerType(NkIrCompiler c);
nktype_t nkir_makeProcedureType(NkIrCompiler c, NkIrProcInfo proc_info);
nktype_t nkir_makeArrayType(NkIrCompiler c, nktype_t elem_t, usize count);
nktype_t nkir_makeVoidType(NkIrCompiler c);
nktype_t nkir_makeAggregateType(NkIrCompiler c, nktype_t const *elem_types, usize const *elem_counts, usize n);

#ifdef __cplusplus
}
#endif

#endif // NKIRC_TYPES_H_
