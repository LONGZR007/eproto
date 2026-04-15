#ifndef LIST_H
#define LIST_H

#include <stddef.h>

// Linux 风格的链表实现
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) \
    { &(name), &(name) }

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr)  \
    do {                     \
        (ptr)->next = (ptr); \
        (ptr)->prev = (ptr); \
    } while (0)

#define list_add(new, head)                    \
    do {                                       \
        struct list_head *prev = (head);       \
        struct list_head *next = (head)->next; \
        (new)->prev = prev;                    \
        (new)->next = next;                    \
        prev->next = (new);                    \
        next->prev = (new);                    \
    } while (0)

#define list_add_tail(new, head)               \
    do {                                       \
        struct list_head *prev = (head)->prev; \
        struct list_head *next = (head);       \
        (new)->prev = prev;                    \
        (new)->next = next;                    \
        prev->next = (new);                    \
        next->prev = (new);                    \
    } while (0)

#define list_del(entry)                         \
    do {                                        \
        struct list_head *prev = (entry)->prev; \
        struct list_head *next = (entry)->next; \
        prev->next = next;                      \
        next->prev = prev;                      \
        (entry)->next = NULL;                   \
        (entry)->prev = NULL;                   \
    } while (0)

#define list_empty(head) ((head)->next == (head))

#define list_entry(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_for_each(pos, head) for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define list_for_each_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

#endif  // LIST_H
