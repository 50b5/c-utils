#ifndef LIST_H
#define LIST_H

#include "map.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct map map;

typedef enum {
    L_TYPE_BOOL,
    L_TYPE_CHAR,
    L_TYPE_DOUBLE,
    L_TYPE_GENERIC,
    L_TYPE_INT,
    L_TYPE_UINT,
    L_TYPE_LIST,
    L_TYPE_MAP,
    L_TYPE_NULL,
    L_TYPE_SIZE_T,
    L_TYPE_STRING,

    L_TYPE_RESERVED_ERROR,
    L_TYPE_RESERVED_EMPTY
} ltype;

typedef void (*list_generic_free)(void *);

typedef struct list_item {
    ltype type;
    size_t size;
    void *data;
    const void *data_copy;
    list_generic_free generic_free;
} list_item;

typedef struct list {
    list_item **items;
    size_t length;
    size_t size;
} list;

list *list_init(void);
list *list_copy(const list *);
bool list_resize(list *, size_t);

size_t list_get_length(const list *);
size_t list_get_size(const list *);
size_t list_get_item_size(const list *, size_t);
/* const char *list_to_string(const list *); */

bool list_contains(const list *, size_t, const void *);
ltype list_get_type(const list *, size_t);
bool list_get_bool(const list *, size_t);
char list_get_char(const list *, size_t);
double list_get_double(const list *, size_t);
int64_t list_get_int(const list *, size_t);
uint64_t list_get_uint(const list *, size_t);
size_t list_get_size_t(const list *, size_t);

/* ------------------ WARNING ------------------
 * the data at these pointers can be modified but
 * the pointer MUST NOT be free'd! the size of the
 * allocated memory stays the same as well. this
 * is so the data can be changed (like modifying
 * a list inside of a list)
 */
char *list_get_string(const list *, size_t);
list *list_get_list(const list *, size_t);
map *list_get_map(const list *, size_t);
void *list_get_generic(const list *, size_t);

bool list_replace(list *, size_t, const list_item *);
bool list_insert(list *, size_t, const list_item *);
bool list_append(list *, const list_item *);

void list_pop(list *, size_t, list_item *);
void list_remove(list *, size_t);
void list_empty(list *);
void list_free(list *);

#endif
