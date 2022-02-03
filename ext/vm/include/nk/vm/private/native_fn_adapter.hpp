#ifndef HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER
#define HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER

#include "nk/common/mem.hpp"
#include "nk/vm/common_types.hpp"

namespace nk {
namespace vm {

struct NativeFnInfo;
using native_fn_info_t = NativeFnInfo *;

native_fn_info_t type_fn_prepareNativeInfo(
    Allocator &allocator,
    void *body_ptr,
    type_t ret_t,
    type_t args_t);

void val_fn_invoke_native(type_t self, value_t ret, value_t args);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER
