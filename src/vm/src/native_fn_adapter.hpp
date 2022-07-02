#ifndef HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER
#define HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER

#include "nk/common/allocator.hpp"
#include "nk/vm/common_types.hpp"

namespace nk {
namespace vm {

void type_fn_prepareNativeInfo(
    Allocator &allocator,
    void *body_ptr,
    size_t argc,
    bool is_variadic,
    FuncPtr &out_body,
    void *&out_closure);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER
