#include "config.hpp"

#include "diagnostics.h"
#include "ntk/file.h"
#include "ntk/string.h"
#include "ntk/sys/error.h"

#define MAX_LINE 4096

bool readConfig(NkHashMap<nks, nks> &conf, NkAllocator alloc, nks file) {
    auto res = nk_file_read(alloc, file);
    if (!res.ok) {
        nkirc_diag_printError(
            "failed to read compiler config `" nks_Fmt "`: %s", nks_Arg(file), nk_getLastErrorString());
        return false;
    }

    nks src = res.bytes;
    size_t lin = 1;
    while (src.size) {
        nks line = nks_trim(nks_chop_by_delim(&src, '\n'));
        if (line.size) {
            if (line.size > MAX_LINE) {
                nkirc_diag_printErrorQuote(res.bytes, file, lin, 0, 0, "failed to read compiler config: line too long");
                return false;
            }
            if (nks_first(line) == '#') {
                continue;
            }
            nks field = nks_chop_by_delim(&line, '=');
            if (!field.size || !line.size) {
                nkirc_diag_printErrorQuote(res.bytes, file, lin, 0, 0, "failed to read compiler config: invalid line");
                return false;
            }
            conf.insert(nk_strcpy(alloc, nks_trim(field)), nk_strcpy(alloc, nks_trim(line)));
        }

        lin++;
    }

    return true;
}
