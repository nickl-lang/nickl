#include "nkl/common/config.h"

#include "nkl/common/diagnostics.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/string.h"

#define MAX_LINE 4096

NK_HASH_TREE_ARRAY_IMPL_KV(nks_config, NkString, NkString, nks_hash, nks_equal);

bool readConfig(nks_config *conf, NkString file) {
    NkString src;
    bool const ok = nk_file_read(conf->alloc, file, &src);
    if (!ok) {
        nkl_diag_printError("failed to read compiler config `" NKS_FMT "`: %s", NKS_ARG(file), nk_getLastErrorString());
        return false;
    }

    usize lin = 1;
    while (src.size) {
        NkString line = nks_trim(nks_chopByDelim(&src, '\n'));
        if (line.size) {
            if (line.size > MAX_LINE) {
                nkl_diag_printErrorQuote(
                    src, (NklSourceLocation){file, lin, 0, 0}, "failed to read compiler config: line too long");
                return false;
            }
            if (NKS_FIRST(line) == '#') {
                continue;
            }
            NkString field = nks_chopByDelim(&line, '=');
            if (!field.size || !line.size) {
                nkl_diag_printErrorQuote(
                    src, (NklSourceLocation){file, lin, 0, 0}, "failed to read compiler config: syntax error");
                return false;
            }
            NkString const field_copy = nks_dupNt(conf->alloc, nks_trim(field));
            NkString const line_copy = nks_dupNt(conf->alloc, nks_trim(line));
            nks_config_insert(conf, field_copy, line_copy);
        }

        lin++;
    }

    return true;
}
