#include "nkl/common/config.h"

#include "nkl/common/diagnostics.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/hash_tree.h"
#include "ntk/string.h"

#define MAX_LINE 4096

static NkString const *NkString_kv_GetKey(NkString_kv const *item) {
    return &item->key;
}

NK_HASH_TREE_IMPL(nks_config, NkString_kv, NkString, NkString_kv_GetKey, nks_hash, nks_equal);

bool readConfig(nks_config *conf, NkString file) {
    NkFileReadResult res = nk_file_read(conf->alloc, file);
    if (!res.ok) {
        nkl_diag_printError("failed to read compiler config `" NKS_FMT "`: %s", NKS_ARG(file), nk_getLastErrorString());
        return false;
    }

    NkString src = res.bytes;
    usize lin = 1;
    while (src.size) {
        NkString line = nks_trim(nks_chopByDelim(&src, '\n'));
        if (line.size) {
            if (line.size > MAX_LINE) {
                nkl_diag_printErrorQuote(
                    res.bytes, (NklSourceLocation){file, lin, 0, 0}, "failed to read compiler config: line too long");
                return false;
            }
            if (nks_first(line) == '#') {
                continue;
            }
            NkString field = nks_chopByDelim(&line, '=');
            if (!field.size || !line.size) {
                nkl_diag_printErrorQuote(
                    res.bytes, (NklSourceLocation){file, lin, 0, 0}, "failed to read compiler config: syntax error");
                return false;
            }
            NkString const field_copy = nks_copyNt(conf->alloc, nks_trim(field));
            NkString const line_copy = nks_copyNt(conf->alloc, nks_trim(line));
            nks_config_insert(conf, (NkString_kv){field_copy, line_copy});
        }

        lin++;
    }

    return true;
}
