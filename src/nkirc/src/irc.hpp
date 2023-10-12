#ifndef HEADER_GUARD_NKB_IRC
#define HEADER_GUARD_NKB_IRC

#include <cstdint>
#include <mutex>

#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/hash_map.hpp"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkIrcColor_Auto,
    NkIrcColor_Always,
    NkIrcColor_Never,
} NkIrcColorPolicy;

typedef struct {
    NkIrcColorPolicy color_policy;
} NkIrcOptions;

typedef struct NkIrCompiler_T {
    NkIrcOptions opts;
    NkIrProg ir;
    NkIrProc entry_point{};
    NkArena tmp_arena{};
    NkArena file_arena{};

    uint8_t usize = sizeof(void *);
    NkHashMap<nks, nktype_t> fpmap{};
    uint64_t next_id{1};
    std::mutex mtx{};
} * NkIrCompiler;

NkIrCompiler nkirc_create(NkIrcOptions opts);
void nkirc_free(NkIrCompiler c);

int nkir_compile(NkIrCompiler c, nks in_file, nks out_file, NkbOutputKind output_kind);
int nkir_run(NkIrCompiler c, nks in_file);

bool nkir_compileFile(NkIrCompiler c, nks base_file, nks in_file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_IRC
