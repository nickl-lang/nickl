#include "ntk/path.h"

#include "ntk/string.h"
#include "ntk/string_builder.h"

static usize commonPrefixLength(char const *lhs, char const *rhs) {
    usize i = 0;
    usize res = 0;

    while (*lhs && *rhs && *lhs == *rhs) {
        i++;
        if (*lhs == NK_PATH_SEPARATOR) {
            res = i;
        }
        lhs++;
        rhs++;
    }

    if ((!*lhs && !*rhs) || (!*lhs && *rhs == NK_PATH_SEPARATOR) || (!*rhs && *lhs == NK_PATH_SEPARATOR)) {
        res = i;
    }

    return res;
}

i32 nk_relativePath(char *buf, usize size, char const *full_path, char const *full_base) {
    usize offset = commonPrefixLength(full_base, full_path);
    if (!offset) {
        buf[0] = '\0';
        return 1;
    }

    char const *path_suffix = full_path + offset;
    char const *base_suffix = full_base + offset;

    if (*base_suffix == NK_PATH_SEPARATOR) {
        base_suffix++;
    }
    if (*path_suffix == NK_PATH_SEPARATOR) {
        path_suffix++;
    }

    NKSB_FIXED_BUFFER_EX(sb, (u8 *)buf, size);

    if (*base_suffix) {
        nksb_printf(&sb, "..");
        for (; *base_suffix; base_suffix++) {
            if (*base_suffix == NK_PATH_SEPARATOR) {
                nksb_printf(&sb, "%c..", NK_PATH_SEPARATOR);
            }
        }
        if (*path_suffix) {
            nksb_printf(&sb, "%c%s", NK_PATH_SEPARATOR, path_suffix);
        }
    } else {
        if (*path_suffix) {
            nksb_printf(&sb, "%s", path_suffix);
        } else {
            nksb_printf(&sb, ".");
        }
    }

    nksb_appendNull(&sb);

    return sb.size;
}

NkString nk_path_getParent(NkString path) {
    nks_chopByDelimReverse(&path, NK_PATH_SEPARATOR);
    return path;
}

NK_EXPORT NkString nk_path_getFilename(NkString path) {
    return nks_chopByDelimReverse(&path, NK_PATH_SEPARATOR);
}

NK_EXPORT NkString nk_path_getExtension(NkString path) {
    NkString name = nk_path_getFilename(path);

    NkString const orig_basename = name;
    NkString ext = nks_chopByDelimReverse(&name, '.');
    if (name.size == 0 && nks_equal(orig_basename, ext)) {
        return (NkString){0};
    }

    char const sep = NK_PATH_SEPARATOR;
    if (nks_endsWith(ext, (NkString){&sep, 1})) {
        ext = nks_left(ext, ext.size - 1);
    }

    return ext;
}
