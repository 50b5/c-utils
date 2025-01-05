#include "list.h"

#include "log.h"
#include "str.h"

#include <stdio.h>
#include <stdlib.h>

#define LIST_MINIMUM_SIZE 8

/*#define LIST_GROWTH_LOAD_FACTOR 1*/
#define LIST_GROWTH_FACTOR 1.5

#define LIST_SHRINK_LOAD_FACTOR 0.25
#define LIST_SHRINK_FACTOR 0.5

static logctx *logger = NULL;

static size_t calculate_new_size(size_t s){
    return (s <= 1 ? s + 1 : s) * LIST_GROWTH_FACTOR;
}

static bool check_availability(list *l){
    size_t newlen = l->length + 1;

    if (newlen >= l->size){
        if (!list_resize(l, calculate_new_size(l->size))){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] check_availability() - list_resize call failed\n",
                __FILE__
            );

            return false;
        }
    }

    return true;
}

static list_item *item_init_pointer(ltype type, size_t size, void *data, list_generic_free generic_free){
    list_item *i = malloc(sizeof(*i));

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] item_init_pointer() - item object alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    i->type = type;
    i->size = size;
    i->data = data;
    i->generic_free = generic_free;

    return i;
}

static list_item *item_init(ltype type, size_t size, const void *data, list_generic_free generic_free){
    list_item *i = malloc(sizeof(*i));

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] item_init() - item object alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    i->type = type;
    i->size = size;
    i->generic_free = generic_free;

    if (type == L_TYPE_LIST){
        i->data = list_copy(data);

        if (!i->data){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] item_init() - list_copy call failed\n",
                __FILE__
            );

            free(i);

            return NULL;
        }
    }
    else if (type == L_TYPE_MAP){
        i->data = map_copy(data);

        if (!i->data){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] item_init() - map_copy call failed\n",
                __FILE__
            );

            free(i);

            return NULL;
        }
    }
    else if (type == L_TYPE_NULL){
        i->data = NULL;
    }
    else if (type == L_TYPE_STRING){
        i->data = malloc(size + 1);

        if (!i->data){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] item_init() - item string alloc failed\n",
                __FILE__
            );

            free(i);

            return NULL;
        }

        string_copy(data, i->data, size);
    }
    else {
        i->data = malloc(size);

        if (!i->data){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] item_init() - item data alloc failed\n",
                __FILE__
            );

            free(i);

            return NULL;
        }

        memcpy(i->data, data, size);
    }

    return i;
}

static void item_free(list_item *i){
    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] item_free() - item should *not* be NULL\n",
            __FILE__
        );

        return;
    }

    switch (i->type){
    case L_TYPE_GENERIC:
        if (i->generic_free){
            i->generic_free(i->data);
        }
        else {
            free(i->data);
        }

        break;
    case L_TYPE_LIST:
        list_free(i->data);

        break;
    case L_TYPE_MAP:
        map_free(i->data);

        break;
    case L_TYPE_NULL:
        break;
    default:
        free(i->data);
    }

    free(i);
}

static list_item *get_item(const list *l, size_t pos, ltype type){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_list_item() - list is NULL\n",
            __FILE__
        );

        return NULL;
    }
    else if (pos >= l->length){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_list_item() - position out of range\n",
            __FILE__
        );

        return NULL;
    }

    list_item *i = l->items[pos];

    if (!i){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_list_item() - unable to get item\n",
            __FILE__
        );
    }
    else if (type != L_TYPE_RESERVED_EMPTY && i->type != type){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_list_item() - item type does *not* match!\n",
            __FILE__
        );
    }

    return i;
}

list *list_init(void){
    if (LIST_MINIMUM_SIZE <= 0){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_init() - LIST_MINIMUM_SIZE must be greater than 0\n",
            __FILE__
        );

        return NULL;
    }

    list *l = calloc(1, sizeof(*l));

    if (!l){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_init() - list object alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    l->length = 0;
    l->size = LIST_MINIMUM_SIZE;
    l->items = calloc(l->size, sizeof(*l->items));

    if (!l->items){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_init() - items object alloc failed\n",
            __FILE__
        );

        free(l);

        return NULL;
    }

    return l;
}

list *list_copy(const list *l){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_copy() - list is NULL\n",
            __FILE__
        );

        return NULL;
    }

    list *copy = list_init();

    if (!copy){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_copy() - list initialization failed\n",
            __FILE__
        );

        return NULL;
    }

    for (size_t index = 0; index < l->length; ++index){
        const list_item *i = get_item(l, index, L_TYPE_RESERVED_EMPTY);

        if (!list_append(copy, i)){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] list_copy() - list_append call failed\n",
                __FILE__
            );

            list_free(copy);

            return NULL;
        }
    }

    return copy;
}

bool list_resize(list *l, size_t size){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_resize() - list is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (size == 0 || size < LIST_MINIMUM_SIZE){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_resize() - size cannot be 0 or less than LIST_MINIMUM_SIZE -- set to LIST_MINIMUM_SIZE (%d)\n",
            __FILE__,
            LIST_MINIMUM_SIZE
        );

        size = LIST_MINIMUM_SIZE;
    }

    if (size < l->length){
        for (size_t index = size; index < l->length; ++index){
            item_free(l->items[index]);
        }

        l->length = size;
    }

    list_item **items = realloc(l->items, size * sizeof(*items));

    if (!items){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_resize() - items object realloc failed\n",
            __FILE__
        );

        return false;
    }

    l->items = items;
    l->size = size;

    return true;
}

size_t list_get_length(const list *l){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_get_length() - list is NULL\n",
            __FILE__
        );

        return 0;
    }

    return l->length;
}

size_t list_get_size(const list *l){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_get_size() - list is NULL\n",
            __FILE__
        );

        return 0;
    }

    return l->size;
}

size_t list_get_item_size(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_RESERVED_EMPTY);

    if (!i){
        return 0;
    }

    return i->size;
}

bool list_contains(const list *l, size_t size, const void *data){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_contains() - list is NULL\n",
            __FILE__
        );

        return false;
    }

    for (size_t index = 0; index < l->length; ++index){
        const list_item *i = get_item(l, index, L_TYPE_RESERVED_EMPTY);

        if (size == i->size && memcmp(data, i->data, i->size)){
            return true;
        }
    }

    return false;
}

ltype list_get_type(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_RESERVED_EMPTY);

    if (!i){
        return L_TYPE_RESERVED_ERROR;
    }

    return i->type;
}

bool list_get_bool(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_BOOL);

    if (!i){
        return false;
    }

    return *(bool *)i->data;
}

char list_get_char(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_CHAR);

    if (!i){
        return 0;
    }

    return *(char *)i->data;
}

double list_get_double(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_DOUBLE);

    if (!i){
        return 0.0;
    }

    return *(double *)i->data;
}

int64_t list_get_int(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_INT);

    if (!i){
        return 0;
    }

    return *(int64_t *)i->data;
}

uint64_t list_get_uint(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_UINT);

    if (!i){
        return 0;
    }

    return *(uint64_t *)i->data;
}

size_t list_get_size_t(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_SIZE_T);

    if (!i){
        return 0;
    }

    return *(size_t *)i->data;
}

/*
 * READ WARNING FOR THESE FUNCTIONS IN HEADER FILE
 */
char *list_get_string(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_STRING);

    if (!i){
        return NULL;
    }

    return i->data;
}

list *list_get_list(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_LIST);

    if (!i){
        return NULL;
    }

    return i->data;
}

map *list_get_map(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_MAP);

    if (!i){
        return NULL;
    }

    return i->data;
}

void *list_get_generic(const list *l, size_t pos){
    const list_item *i = get_item(l, pos, L_TYPE_GENERIC);

    if (!i){
        return NULL;
    }

    return i->data;
}

bool list_replace(list *l, size_t pos, const list_item *item){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_replace() - list is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!item){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_replace() - item is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (pos >= l->length){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_replace() - position %ld is out of bounds\n",
            __FILE__,
            pos
        );

        return false;
    }

    list_item *i = NULL;

    if (item->data){
        i = item_init_pointer(
            item->type,
            item->size,
            item->data,
            item->generic_free
        );
    }
    else {
        i = item_init(
            item->type,
            item->size,
            item->data_copy,
            item->generic_free
        );
    }

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_replace() - item object initialization failed\n",
            __FILE__
        );

        return false;
    }

    item_free(l->items[pos]);

    l->items[pos] = i;

    return true;
}

bool list_insert(list *l, size_t pos, const list_item *item){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_insert() - list is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!item){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_insert() - item is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (pos > l->length){
        return list_append(l, item);
    }

    if (!check_availability(l)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_insert() - check_availability call failed\n",
            __FILE__
        );

        return false;
    }

    list_item *i = NULL;

    if (item->data){
        i = item_init_pointer(
            item->type,
            item->size,
            item->data,
            item->generic_free
        );
    }
    else {
        i = item_init(
            item->type,
            item->size,
            item->data_copy,
            item->generic_free
        );
    }

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_insert() - item object initialization failed\n",
            __FILE__
        );

        return false;
    }

    for (size_t index = l->length; index > pos; --index){
        l->items[index] = l->items[index - 1];
    }

    l->items[pos] = i;
    l->length += 1;

    return true;
}

bool list_append(list *l, const list_item *item){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_append() - list is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!item){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_append() - item is NULL\n",
            __FILE__
        );

        return false;
    }

    if (!check_availability(l)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_append() - check_availability call failed\n",
            __FILE__
        );

        return false;
    }

    list_item *i = NULL;

    if (item->data){
        i = item_init_pointer(
            item->type,
            item->size,
            item->data,
            item->generic_free
        );
    }
    else {
        i = item_init(
            item->type,
            item->size,
            item->data_copy,
            item->generic_free
        );
    }

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_append() - item object initialization failed\n",
            __FILE__
        );

        return false;
    }

    l->items[l->length++] = i;

    return true;
}

void list_pop(list *l, size_t pos, list_item *item){
    list_item *i = get_item(l, pos, L_TYPE_RESERVED_EMPTY);

    if (!i){
        return;
    }

    if (item){
        item->type = i->type;
        item->size = i->size;
        item->data = i->data;
        item->generic_free = i->generic_free;

        if (item->data){
            i->type = L_TYPE_NULL;
            i->size = 0;
            i->data = NULL;
            i->generic_free = NULL;
        }
    }
    else {
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] list_pop() - item is NULL -- removing but unable to assign\n",
            __FILE__
        );
    }

    list_remove(l, pos);
}

void list_remove(list *l, size_t pos){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_remove() - list is NULL\n",
            __FILE__
        );

        return;
    }
    else if (pos >= l->length){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_remove() - position %ld is out of bounds\n",
            __FILE__,
            pos
        );

        return;
    }

    item_free(l->items[pos]);

    for (size_t index = pos; index < l->length; ++index){
        l->items[index] = l->items[index + 1];
    }

    --l->length;

    if (l->size > LIST_MINIMUM_SIZE){
        double load = (double)l->length / (double)l->size;

        if (load <= LIST_SHRINK_LOAD_FACTOR){
            size_t newsize = l->size - ((l->size - l->length) * LIST_SHRINK_FACTOR);

            if (newsize >= l->size){
                log_write(
                    logger,
                    LOG_WARNING,
                    "[%s] list_remove() - newsize (%ld) >= l->size (%ld) -- unable to shrink list\n",
                    __FILE__,
                    newsize,
                    l->size
                );

                return;
            }

            if (!list_resize(l, newsize)){
                return;
            }
        }
    }
}

void list_empty(list *l){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_empty() - list is NULL\n",
            __FILE__
        );

        return;
    }

    for (size_t index = 0; index < l->length; ++index){
        list_remove(l, index);
    }
}

void list_free(list *l){
    if (!l){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] list_free() - list is NULL\n",
            __FILE__
        );

        return;
    }

    for (size_t index = 0; index < l->length; ++index){
        item_free(l->items[index]);
    }

    free(l->items);
    free(l);
}
