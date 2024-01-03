#include "ntk/hash_tree.h"

#include <stdalign.h>
#include <string.h>

#include "ntk/common.h"
#include "ntk/utils.h"

typedef struct {
    uint64_t hash;
    void *child[2];
} NodeInfo;

#define node_info_ptr(node, elemsize) ((NodeInfo *)roundUp((size_t)((uint8_t *)(node) + (elemsize)), alignof(NodeInfo)))
#define node_size(elemsize) roundUp((elemsize), alignof(NodeInfo)) + sizeof(NodeInfo)

#define hash_key(key, keysize) hash_array((uint8_t *)(key), (uint8_t *)(key) + (keysize))
#define key_equal(lhs, rhs, keysize) (memcmp((lhs), (rhs), (keysize)) == 0)

typedef struct {
    void **node;
    bool existing;
} SearchResult;

static SearchResult findNode(void **node, size_t elemsize, void *key, size_t keysize, size_t keyoffset, uint64_t hash) {
    while (*node) {
        NodeInfo *node_info = node_info_ptr(*node, elemsize);
        switch ((hash > node_info->hash) - (hash < node_info->hash)) {
        case 0: {
            if (key_equal(key, (uint8_t *)*node + keyoffset, keysize)) {
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

void *_nkht_insert_impl(void **root, NkAllocator alloc, void *elem, size_t elemsize, size_t keysize, size_t keyoffset) {
    void *key = (uint8_t *)elem + keyoffset;
    uint64_t hash = hash_key(key, keysize);
    SearchResult res = findNode(root, elemsize, key, keysize, keyoffset, hash);
    if (!res.existing) {
        NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
        *res.node = nk_alloc(_alloc, node_size(elemsize));
        memcpy(*res.node, elem, elemsize);
        NodeInfo *new_node_info = node_info_ptr(*res.node, elemsize);
        *new_node_info = (NodeInfo){
            .hash = hash,
            .child = {0},
        };
    }
    return *res.node;
}

void *_nkht_find_impl(void *root, size_t elemsize, void *key, size_t keysize, size_t keyoffset) {
    uint64_t hash = hash_key(key, keysize);
    SearchResult res = findNode(&root, elemsize, key, keysize, keyoffset, hash);
    return *res.node;
}

static void freeNode(NkAllocator alloc, void *node, size_t elemsize) {
    if (!node) {
        return;
    }
    NodeInfo *node_info = node_info_ptr(node, elemsize);
    freeNode(alloc, node_info->child[0], elemsize);
    freeNode(alloc, node_info->child[1], elemsize);
    nk_free(alloc, node, node_size(elemsize));
}

void _nkht_free_impl(void **root, NkAllocator alloc, size_t elemsize) {
    NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
    freeNode(_alloc, *root, elemsize);
    *root = NULL;
}
