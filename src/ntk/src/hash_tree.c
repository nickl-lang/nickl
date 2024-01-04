#include "ntk/hash_tree.h"

#include <stdalign.h>
#include <string.h>

#include "ntk/array.h"
#include "ntk/common.h"
#include "ntk/utils.h"

typedef struct {
    uint64_t hash;
    void *child[2];
} NodeInfo;

#define node_info_ptr(node, elem_size) \
    ((NodeInfo *)roundUp((size_t)((uint8_t *)(node) + (elem_size)), alignof(NodeInfo)))
#define node_size(elem_size) roundUp((elem_size), alignof(NodeInfo)) + sizeof(NodeInfo)

#define hash_key(key, key_size) hash_array((uint8_t *)(key), (uint8_t *)(key) + (key_size))
#define key_equal(lhs, rhs, key_size) (memcmp((lhs), (rhs), (key_size)) == 0)

static void *key_data_ptr(void *elem, size_t key_offset, _nkht_hash_mode mode) {
    void *key = (uint8_t *)elem + key_offset;
    switch (mode) {
    default:
    case _nkht_hash_val:
        return key;
    case _nkht_hash_str: {
        return *(void **)key;
    }
    }
}

typedef struct {
    void **node;
    bool existing;
} SearchResult;

static SearchResult findNode(
    void **node,
    size_t elem_size,
    void *key,
    size_t key_size,
    size_t key_offset,
    uint64_t hash,
    _nkht_hash_mode mode) {
    while (*node) {
        NodeInfo *node_info = node_info_ptr(*node, elem_size);
        switch ((hash > node_info->hash) - (hash < node_info->hash)) {
        case 0: {
            void *existing_key = key_data_ptr(*node, key_offset, mode);
            if (key_equal(key, existing_key, key_size)) {
                return (SearchResult){node, true};
            }
        } /*fallthrough*/
        case -1:
            node = node_info->child + 0;
            break;
        case +1:
            node = node_info->child + 1;
            break;
        }
    }
    return (SearchResult){node, false};
}

void *_nkht_insert_impl(
    void **root,
    NkAllocator alloc,
    void *elem,
    size_t elem_size,
    size_t key_size,
    size_t key_offset,
    _nkht_hash_mode mode) {
    void *key = key_data_ptr(elem, key_offset, mode);
    uint64_t hash = hash_key(key, key_size);
    SearchResult res = findNode(root, elem_size, key, key_size, key_offset, hash, mode);
    if (!res.existing) {
        NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
        *res.node = nk_alloc(_alloc, node_size(elem_size));
        memcpy(*res.node, elem, elem_size);
        NodeInfo *new_node_info = node_info_ptr(*res.node, elem_size);
        *new_node_info = (NodeInfo){
            .hash = hash,
            .child = {0},
        };
    }
    return *res.node;
}

void *_nkht_find_impl(
    void *root,
    size_t elem_size,
    void *key,
    size_t key_size,
    size_t key_offset,
    _nkht_hash_mode mode) {
    uint64_t hash = hash_key(key, key_size);
    SearchResult res = findNode(&root, elem_size, key, key_size, key_offset, hash, mode);
    return *res.node;
}

static void freeNode(NkAllocator alloc, void *node, size_t elem_size) {
    if (!node) {
        return;
    }
    NodeInfo *node_info = node_info_ptr(node, elem_size);
    freeNode(alloc, node_info->child[0], elem_size);
    freeNode(alloc, node_info->child[1], elem_size);
    nk_free(alloc, node, node_size(elem_size));
}

void _nkht_free_impl(void **root, NkAllocator alloc, size_t elem_size) {
    NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
    freeNode(_alloc, *root, elem_size);
    *root = NULL;
}
