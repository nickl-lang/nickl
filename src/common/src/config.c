#include "nkl/common/config.h"

#include "nkl/common/diagnostics.h"
#include "ntk/file.h"
#include "ntk/hash_tree.h"
#include "ntk/string.h"
#include "ntk/sys/error.h"

#define MAX_LINE 4096

static nks const *nkv_KV_GetKey(nks_KV const *item) {
    return &item->key;
}

nkht_impl(nks_config, nks_KV, nks, nkv_KV_GetKey, nks_hash, nks_equal);

bool readConfig(nks_config *conf, nks file) {
    NkFileReadResult res = nk_file_read(conf->alloc, file);
    if (!res.ok) {
        nkl_diag_printError("failed to read compiler config `" nks_Fmt "`: %s", nks_Arg(file), nk_getLastErrorString());
        return false;
    }

    nks src = res.bytes;
    size_t lin = 1;
    while (src.size) {
        nks line = nks_trim(nks_chop_by_delim(&src, '\n'));
        if (line.size) {
            if (line.size > MAX_LINE) {
                nkl_diag_printErrorQuote(
                    res.bytes, (NklSourceLocation){file, lin, 0, 0}, "failed to read compiler config: line too long");
                return false;
            }
            if (nks_first(line) == '#') {
                continue;
            }
            nks field = nks_chop_by_delim(&line, '=');
            if (!field.size || !line.size) {
                nkl_diag_printErrorQuote(
                    res.bytes, (NklSourceLocation){file, lin, 0, 0}, "failed to read compiler config: syntax error");
                return false;
            }
            nks const field_copy = nk_strcpy_nt(conf->alloc, nks_trim(field));
            nks const line_copy = nk_strcpy_nt(conf->alloc, nks_trim(line));
            nks_config_insert(conf, (nks_KV){field_copy, line_copy});
        }

        lin++;
    }

    return true;
}
