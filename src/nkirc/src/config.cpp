#include "config.hpp"

#include "ntk/file.h"
#include "ntk/string.h"

bool readConfig(NkHashMap<nks, nks> &conf, NkAllocator alloc, nks file) {
    auto res = nk_file_read(alloc, file);
    if (!res.ok) {
        return false;
    }

    while (res.bytes.size) {
        nks line = nks_chop_by_delim(&res.bytes, '\n');
        if (line.size) {
            nks field = nks_chop_by_delim(&line, '=');
            if (!field.size || !line.size) {
                return false;
            }
            conf.insert(nk_strcpy(alloc, nks_trim(field)), nk_strcpy(alloc, nks_trim(line)));
        }
    }

    return true;
}
