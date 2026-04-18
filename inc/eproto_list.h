/*
 * MIT License
 *
 * Copyright (c) 2026 LONGZR007
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef EPROTO_LIST_H
#define EPROTO_LIST_H

#include <stddef.h>

struct eproto_list_head {
    struct eproto_list_head *next, *prev;
};

#define EPROTO_LIST_HEAD_INIT(name) \
    { &(name), &(name) }

#define EPROTO_LIST_HEAD(name) struct eproto_list_head name = EPROTO_LIST_HEAD_INIT(name)

#define EPROTO_INIT_LIST_HEAD(ptr) \
    do {                           \
        (ptr)->next = (ptr);       \
        (ptr)->prev = (ptr);       \
    } while (0)

#define eproto_list_add(new, head)                    \
    do {                                              \
        struct eproto_list_head *prev = (head);       \
        struct eproto_list_head *next = (head)->next; \
        (new)->prev = prev;                           \
        (new)->next = next;                           \
        prev->next = (new);                           \
        next->prev = (new);                           \
    } while (0)

#define eproto_list_add_tail(new, head)               \
    do {                                              \
        struct eproto_list_head *prev = (head)->prev; \
        struct eproto_list_head *next = (head);       \
        (new)->prev = prev;                           \
        (new)->next = next;                           \
        prev->next = (new);                           \
        next->prev = (new);                           \
    } while (0)

#define eproto_list_del(entry)                         \
    do {                                               \
        struct eproto_list_head *prev = (entry)->prev; \
        struct eproto_list_head *next = (entry)->next; \
        prev->next = next;                             \
        next->prev = prev;                             \
        (entry)->next = NULL;                          \
        (entry)->prev = NULL;                          \
    } while (0)

#define eproto_list_empty(head) ((head)->next == (head))

#define eproto_list_entry(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define eproto_list_for_each(pos, head) for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define eproto_list_for_each_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

#endif
