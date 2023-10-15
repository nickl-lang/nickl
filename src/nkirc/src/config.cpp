#include "config.hpp"

#include "diagnostics.h"
#include "ntk/file.h"
#include "ntk/string.h"
#include "ntk/sys/error.h"

bool readConfig(NkHashMap<nks, nks> &conf, NkAllocator alloc, nks file) {
    auto res = nk_file_read(alloc, file);
    if (!res.ok) {
        nkirc_diag_printError(
            "failed to read compiler config `" nks_Fmt "`: %s", nks_Arg(file), nk_getLastErrorString());
        return false;
    }

    size_t lin = 1;
    while (res.bytes.size) {
        nks line = nks_trim(nks_chop_by_delim(&res.bytes, '\n'));
        if (line.size) {
            if (nks_first(line) == '#') {
                continue;
            }
            nks field = nks_chop_by_delim(&line, '=');
            if (!field.size || !line.size) {
                nkirc_diag_printError(
                    "failed to read compiler config `" nks_Fmt "`: error at line %zu", nks_Arg(file), lin);
                return false;
            }
            conf.insert(nk_strcpy(alloc, nks_trim(field)), nk_strcpy(alloc, nks_trim(line)));
        }

        lin++;
    }

    return true;
}
