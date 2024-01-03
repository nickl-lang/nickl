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

void *_nkht_insert_impl(void **root, NkAllocator alloc, void *elem, size_t elemsize, size_t keysize, size_t keyoffset) {
    void *key = (uint8_t *)elem + keyoffset;
    uint64_t hash = hash_key(key, keysize);
    void **insert = root;
    while (*insert) {
        NodeInfo *node_info = node_info_ptr(*insert, elemsize);
        switch ((hash > node_info->hash) - (hash < node_info->hash)) {
        case 0:
            if (key_equal(key, (uint8_t *)*insert + keyoffset, keysize)) {
                return *insert;
            } /*fallthrough*/
        case -1:
            insert = node_info->child + 0;
            break;
        case +1:
            insert = node_info->child + 1;
            break;
        }
    }
    NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
    *insert = nk_alloc(_alloc, node_size(elemsize));
    memcpy(*insert, elem, elemsize);
    NodeInfo *new_node_info = node_info_ptr(*insert, elemsize);
    *new_node_info = (NodeInfo){
        .hash = hash,
        .child = {0},
    };
    return *insert;
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

void _nkht_free_impl(void *root, NkAllocator alloc, size_t elemsize) {
    NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
    freeNode(_alloc, root, elemsize);
}
