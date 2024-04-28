#ifndef NTK_LIST_H_
#define NTK_LIST_H_

#define nk_list_push_n(list, item, next) ((item)->next = (list), (list) = (item))
#define nk_list_push(list, item) nk_list_push_n(list, item, next)

#define nk_list_pop_n(list, next) ((list) = (list)->next)
#define nk_list_pop(list) nk_list_pop_n(list, next)

#endif // NTK_LIST_H_
