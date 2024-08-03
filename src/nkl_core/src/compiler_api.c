#include "nkl/core/compiler_api.h"

#include "nickl_impl.h"

StringSlice nickl_api_getCommandLineArgs(NklState nkl) {
    return nkl->cli_args;
}
