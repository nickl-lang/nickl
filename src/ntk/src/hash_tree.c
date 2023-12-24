#include "ntk/hash_tree.h"

#include <stdalign.h>
#include <string.h>

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

void *_nkht_insert_impl(
    void **insert,
    NkAllocator alloc,
    void *elem,
    size_t elem_size,
    size_t key_size,
    size_t key_offset) {
    void *key = (uint8_t *)elem + key_offset;
    uint64_t hash = hash_key(key, key_size);
    while (*insert) {
        NodeInfo *node_info = node_info_ptr(*insert, elem_size);
        switch ((hash > node_info->hash) - (hash < node_info->hash)) {
        case 0:
            if (key_equal(key, (uint8_t *)*insert + key_offset, key_size)) {
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
    *insert = nk_alloc(_alloc, node_size(elem_size));
    memcpy(*insert, elem, elem_size);
    NodeInfo *new_node_info = node_info_ptr(*insert, elem_size);
    *new_node_info = (NodeInfo){
        .hash = hash,
        .child = {0},
    };
    return *insert;
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

void _nkht_free_impl(void *root, NkAllocator alloc, size_t elem_size) {
    NkAllocator _alloc = alloc.proc ? alloc : nk_default_allocator;
    freeNode(_alloc, root, elem_size);
}
