#include "nkl/core/search.h"

#include "ntk/dl.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(search);

} // namespace

NkHandle nkl_findLibrary(NkAtom name) {
    NK_PROF_FUNC();

    // TODO: Cache found libraries
    auto const name_str = nk_atom2cs(name);
    auto h_lib = nkdl_loadLibrary(name_str);
    if (nk_handleIsNull(h_lib)) {
        // TODO: Fixed buffer, use scratch arena
        NKSB_FIXED_BUFFER(err_str, 1024);
        nksb_printf(&err_str, "%s", nkdl_getLastErrorString());

        // TODO: Fixed buffer, use scratch arena
        NKSB_FIXED_BUFFER(lib_name, 1024);
        nksb_printf(&lib_name, "%s.%s", name_str, nkdl_file_extension);
        nksb_appendNull(&lib_name);
        h_lib = nkdl_loadLibrary(lib_name.data);

        if (nk_handleIsNull(h_lib)) {
            nksb_clear(&lib_name);

            nksb_printf(&lib_name, "lib%s", name_str);
            nksb_appendNull(&lib_name);
            h_lib = nkdl_loadLibrary(lib_name.data);

            if (nk_handleIsNull(h_lib)) {
                nksb_clear(&lib_name);

                nksb_printf(&lib_name, "lib%s.%s", name_str, nkdl_file_extension);
                nksb_appendNull(&lib_name);
                h_lib = nkdl_loadLibrary(lib_name.data);

                if (nk_handleIsNull(h_lib)) {
                    // TODO: Report error to the user instead of logging
                    NK_LOG_ERR("failed to load extern library `%s`: " NKS_FMT, name_str, NKS_ARG(err_str));
                    return NK_NULL_HANDLE;
                }
            }
        }
    }
    NK_LOG_DBG("loaded extern library `%s`: %zu", name_str, h_lib.val);
    return h_lib;
}
