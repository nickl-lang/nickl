#include "nk/vm/interp.hpp"

#include "nk/common/logger.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::interp)

} // namespace

void interp_init(Program const *prog) {
}

void interp_invoke(FunctInfo const &info, value_t ret, value_t args) {
    LOG_TRC("interp_invoke()");
}

} // namespace vm
} // namespace nk
