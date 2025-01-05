#include "map.h"

#include "log.h"
#include "str.h"

#include "hashers/spooky.h"

#include <stdio.h>
#include <stdlib.h>

#define MAP_MINIMUM_SIZE 8
#define MAP_GROWTH_LOAD_FACTOR 0.8

#define LCG_MULTIPLIER 6364136223846793005
#define LCG_INCREMENT 1

static logctx *logger = NULL;

typedef struct node {
    uint32_t hash;

    map_item *key;
    map_item *value;

    node *prev;
    node *next;
} node;

static bool is_power_of_two(size_t number){
    return number && !(number & (number - 1));
}

static uint32_t generate_hash(uint32_t seed, size_t size, const void *data){
    return spooky_hash32(data, size, seed);
}

static size_t generate_index(uint32_t seed, size_t size){
    size_t multiplier = LCG_MULTIPLIER;
    size_t increment = LCG_INCREMENT;

    return (multiplier * seed + increment) & (size - 1);
}

static bool check_availability(map *m){
    double load = (double)m->length / (double)m->size;

    if (load >= MAP_GROWTH_LOAD_FACTOR){
        size_t newsize = m->size << 1;

        if (newsize <= m->size){
            log_write(
                logger,
                LOG_WARNING,
                "[%s] map_set() - newsize (%ld) <= m->size (%ld) -- unable to grow map\n",
                __FILE__,
                newsize,
                m->size
            );

            return false;
        }

        if (!map_resize(m, newsize)){
            return false;
        }
    }

    return true;
}

static map_item *item_init_pointer(mtype type, size_t size, void *data, map_generic_free generic_free){
    map_item *i = malloc(sizeof(*i));

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] item_init_pointer() - item alloc failed\n",
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

static map_item *item_init(mtype type, size_t size, const void *data, map_generic_free generic_free){
    map_item *i = malloc(sizeof(*i));

    if (!i){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] item_init() - item alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    i->type = type;
    i->size = size;
    i->generic_free = generic_free;

    if (type == M_TYPE_STRING){
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
    else if (type == M_TYPE_LIST){
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
    else if (type == M_TYPE_MAP){
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
    else if (type == M_TYPE_NULL){
        i->data = NULL;
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

static void item_free(map_item *i){
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
    case M_TYPE_GENERIC:
        if (i->generic_free){
            i->generic_free(i->data);
        }
        else {
            free(i->data);
        }

        break;
    case M_TYPE_LIST:
        list_free(i->data);

        break;
    case M_TYPE_MAP:
        map_free(i->data);

        break;
    case M_TYPE_NULL:
        break;
    default:
        free(i->data);
    }

    free(i);
}

static node *node_init(const map_item *key, const map_item *value){
    node *n = calloc(1, sizeof(*n));

    if (!n){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] node_init() - node alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    n->key = item_init(key->type, key->size, key->data_copy, key->generic_free);

    if (!n->key){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] node_init() - key initialization failed\n",
            __FILE__
        );

        free(n);

        return NULL;
    }

    if (value->data){
        n->value = item_init_pointer(
            value->type,
            value->size,
            value->data,
            value->generic_free
        );
    }
    else {
        n->value = item_init(
            value->type,
            value->size,
            value->data_copy,
            value->generic_free
        );
    }

    if (!n->value){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] node_init() - value initialization failed\n",
            __FILE__
        );

        item_free(n->key);
        free(n);

        return NULL;
    }

    return n;
}

static void node_free(node *n){
    if (!n){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] node_free() - node should *not* be NULL\n",
            __FILE__
        );

        return;
    }

    item_free(n->key);
    item_free(n->value);

    free(n);
}

static bool get_node_index(const map *m, size_t *ret, size_t size, const void *key){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_node_index() - map is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!key){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_node_index() - key is NULL\n",
            __FILE__
        );

        return false;
    }

    uint32_t hash = generate_hash(m->seed, size, key);
    size_t index = generate_index(hash, m->size);
    node *n = m->nodes[index];

    bool found = false;

    for (size_t count = 0; count < m->size; ++count){
        if (n && hash == n->hash && !memcmp(key, n->key->data, n->key->size)){
            found = true;

            break;
        }

        index = generate_index(index, m->size);
        n = m->nodes[index];
    }

    if (!found){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] get_node_index() - key does not exist\n",
            __FILE__
        );

        return false;
    }

    *ret = index;

    return true;
}

static node *get_node(const map *m, size_t size, const void *key, mtype type){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_node() - map is NULL\n",
            __FILE__
        );

        return NULL;
    }
    else if (!key){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_node() - key is NULL\n",
            __FILE__
        );

        return NULL;
    }

    size_t index;

    if (!get_node_index(m, &index, size, key)){
        return NULL;
    }

    node *n = m->nodes[index];

    if (type != M_TYPE_RESERVED_EMPTY && n->value->type != type){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] get_node() - node type does *not* match\n",
            __FILE__
        );
    }

    return n;
}

map *map_init(void){
    if (MAP_MINIMUM_SIZE <= 0){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_init() - MAP_MINIMUM_SIZE must be greater than 0\n",
            __FILE__
        );

        return NULL;
    }
    else if (!is_power_of_two(MAP_MINIMUM_SIZE)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_init() - MAP_MINIMUM_SIZE must be a power of 2\n",
            __FILE__
        );

        return NULL;
    }


    map *m = calloc(1, sizeof(*m));

    if (!m){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_init() - map alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    m->length = 0;
    m->size = MAP_MINIMUM_SIZE;
    m->nodes = calloc(m->size, sizeof(*m->nodes));

    if (!m->nodes){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_init() - nodes alloc failed\n",
            __FILE__
        );

        free(m);

        return NULL;
    }

    m->seed = (uint32_t)&m;

    m->first = NULL;
    m->last = NULL;

    return m;
}

map *map_copy(const map *m){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_copy() - map is NULL\n",
            __FILE__
        );

        return NULL;
    }

    map *copy = map_init();

    if (!copy){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_copy() - map intiialization failed\n",
            __FILE__
        );

        return NULL;
    }

    node *n = m->first;

    while (n){
        const map_item *k = n->key;
        const map_item *v = n->value;

        if (!map_set(copy, k, v)){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] map_copy() - map_set call failed\n",
                __FILE__
            );

            map_free(copy);

            return NULL;
        }

        n = n->next;
    }

    return copy;
}

bool map_resize(map *m, size_t size){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_resize() - map is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (size == 0 || size < MAP_MINIMUM_SIZE){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_resize() - size cannot be 0 or less than MAP_MINIMUM_SIZE -- set to MAP_MINIMUM_SIZE (%d)\n",
            __FILE__,
            MAP_MINIMUM_SIZE
        );

        size = MAP_MINIMUM_SIZE;
    }
    else if (!is_power_of_two(size)){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_resize() - size must be a power of 2\n",
            __FILE__
        );

        return false;
    }

    node **nodes = calloc(size, sizeof(*nodes));

    if (!nodes){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_resize() - nodes alloc failed\n",
            __FILE__
        );

        return false;
    }

    for (size_t index = 0; index < m->size; ++index){
        node *n = m->nodes[index];

        if (!n){
            continue;
        }

        if (index > size){
            if (n->prev){
                n->prev->next = n->next;
            }

            node_free(n);

            --m->length;

            continue;
        }

        size_t newindex = generate_index(n->hash, size);
        node *hold = nodes[newindex];

        for (size_t count = 0; count < size; ++count){
            if (!hold){
                break;
            }

            newindex = generate_index(newindex, size);
            hold = nodes[newindex];
        }

        nodes[newindex] = n;
    }

    free(m->nodes);

    m->nodes = nodes;
    m->size = size;

    return true;
}

size_t map_get_length(const map *m){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_get_length() - map is NULL\n",
            __FILE__
        );

        return 0;
    }

    return m->length;
}

size_t map_get_size(const map *m){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_get_size() - map is NULL\n",
            __FILE__
        );

        return 0;
    }

    return m->size;
}

mapiter *map_iter_init(const map *m){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_init() - map is NULL\n",
            __FILE__
        );

        return NULL;
    }

    mapiter *iter = malloc(sizeof(*iter));

    if (!iter){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_iter_init() - iterator alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    iter->m = m;
    iter->n = NULL;

    return iter;
}

bool map_iter_is_last(const mapiter *iter){
    if (!iter){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_is_last() - iterator is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_is_last() - map is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->n){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] map_iter_is_last() - node is NULL -- map_iter_next hasn't been called or map is empty\n",
            __FILE__
        );

        return false;
    }

    return iter->n == iter->m->last;
}

bool map_iter_get_key(const mapiter *iter, map_item *key){
    if (!iter){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_get_key() - iterator is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_get_key() - map is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->n){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] map_iter_get_key() - node is NULL -- map_iter_next hasn't been called or map is empty\n",
            __FILE__
        );

        return false;
    }
    else if (!key){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_get_key() - key is NULL -- unable to assign\n",
            __FILE__
        );

        return false;
    }

    *key = *iter->n->key;

    return true;
}

bool map_iter_get_value(const mapiter *iter, map_item *value){
    if (!iter){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_get_value() - iterator is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_get_value() - map is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->n){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] map_iter_get_value() - node is NULL -- map_iter_next hasn't been called or map is empty\n",
            __FILE__
        );

        return false;
    }
    else if (!value){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_get_value() - value is NULL -- unable to assign\n",
            __FILE__
        );

        return false;
    }

    *value = *iter->n->value;

    return true;
}

bool map_iter_next(mapiter *iter){
    if (!iter){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_next() - iterator is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_next() - map is NULL\n",
            __FILE__
        );

        return false;
    }

    iter->n = iter->n ? iter->n->next : iter->m->first;

    return iter->n ? true : false;
}

bool map_iter_prev(mapiter *iter){
    if (!iter){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_prev() - iterator is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!iter->m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_iter_prev() - map is NULL\n",
            __FILE__
        );

        return false;
    }

    iter->n = iter->n ? iter->n->prev : iter->m->last;

    return iter->n ? true : false;
}

void map_iter_free(mapiter *iter){
    if (!iter){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] map_iter_free() - iterator is NULL\n",
            __FILE__
        );

        return;
    }

    free(iter);
}

bool map_contains(const map *m, size_t size, const void *key){
    return get_node(m, size, key, M_TYPE_RESERVED_EMPTY);
}

mtype map_get_type(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_RESERVED_EMPTY);

    if (!n){
        return M_TYPE_RESERVED_ERROR;
    }

    return n->value->type;
}

bool map_get_bool(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_BOOL);

    if (!n){
        return false;
    }

    return *(bool *)n->value->data;
}

char map_get_char(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_CHAR);

    if (!n){
        return 0;
    }

    return *(char *)n->value->data;
}

double map_get_double(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_DOUBLE);

    if (!n){
        return 0.0;
    }

    return *(double *)n->value->data;
}

int64_t map_get_int(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_INT);

    if (!n){
        return 0;
    }

    return *(int64_t *)n->value->data;
}

uint64_t map_get_uint(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_UINT);

    if (!n){
        return 0;
    }

    return *(uint64_t *)n->value->data;
}

size_t map_get_size_t(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_SIZE_T);

    if (!n){
        return 0;
    }

    return *(size_t *)n->value->data;
}

/*
 * READ WARNING FOR THESE FUNCTIONS IN HEADER FILE
 */
char *map_get_string(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_STRING);

    if (!n){
        return NULL;
    }

    return n->value->data;
}

list *map_get_list(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_LIST);

    if (!n){
        return NULL;
    }

    return n->value->data;
}

map *map_get_map(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_MAP);

    if (!n){
        return NULL;
    }

    return n->value->data;
}

void *map_get_generic(const map *m, size_t size, const void *key){
    const node *n = get_node(m, size, key, M_TYPE_GENERIC);

    if (!n){
        return NULL;
    }

    return n->value->data;
}

bool map_set(map *m, const map_item *key, const map_item *value){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_set() - map is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!key){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_set() - key is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (key->data){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_set() - key will *always* be copied -- set key in data_copy instead\n",
            __FILE__
        );

        return false;
    }

    if (!check_availability(m)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_set() - check_availability call failed\n",
            __FILE__
        );

        return false;
    }

    uint32_t hash = generate_hash(m->seed, key->size, key->data_copy);
    size_t index = generate_index(hash, m->size);
    node *n = m->nodes[index];

    for (size_t count = 0; count < m->size; ++count){
        if (!n){
            break;
        }

        if (hash == n->hash && !memcmp(key->data_copy, n->key->data, n->key->size)){
            map_item *tmp = NULL;

            if (value->data){
                tmp = item_init_pointer(
                    value->type,
                    value->size,
                    value->data,
                    value->generic_free
                );
            }
            else {
                tmp = item_init(
                    value->type,
                    value->size,
                    value->data_copy,
                    value->generic_free
                );
            }

            if (!tmp){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] map_set() - item initialization failed\n",
                    __FILE__
                );

                return false;
            }

            item_free(n->value);

            n->value = tmp;

            return true;
        }

        index = generate_index(index, m->size);
        n = m->nodes[index];
    }

    n = node_init(key, value);

    if (!n){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_set() - node initialization failed\n",
            __FILE__
        );

        return false;
    }

    n->hash = hash;
    m->nodes[index] = n;

    ++m->length;

    if (m->first){
        n->prev = m->last;
        m->last->next = n;
        m->last = n;
    }
    else {
        m->first = n;
        m->last = n;
    }

    return true;
}

void map_pop(map *m, size_t size, const void *key, map_item *value){
    node *n = get_node(m, size, key, M_TYPE_RESERVED_EMPTY);

    if (!n){
        return;
    }

    if (value){
        value->type = n->value->type;
        value->size = n->value->size;
        value->data = n->value->data;
        value->generic_free = n->value->generic_free;

        if (value->data){
            n->value->type = M_TYPE_NULL;
            n->value->size = 0;
            n->value->data = NULL;
            n->value->generic_free = NULL;
        }
    }
    else {
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] map_pop() - value is NULL -- removing but unable to assign\n",
            __FILE__
        );
    }

    map_remove(m, size, key);
}

void map_remove(map *m, size_t size, const void *key){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_remove() - map is NULL\n",
            __FILE__
        );

        return;
    }
    else if (!key){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_remove() - key is NULL\n",
            __FILE__
        );

        return;
    }

    size_t index;

    if (!get_node_index(m, &index, size, key)){
        return;
    }

    node *n = m->nodes[index];

    if (n == m->first){
        m->first = n->next;
    }
    else if (n == m->last){
        m->last = n->prev;
    }

    if (n->next){
        n->next->prev = n->prev;
    }

    if (n->prev){
        n->prev->next = n->next;
    }

    node_free(n);

    m->nodes[index] = NULL;

    --m->length;
}

void map_free(map *m){
    if (!m){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] map_free() - map is NULL\n",
            __FILE__
        );

        return;
    }

    for (size_t index = 0; index < m->size; ++index){
        node *n = m->nodes[index];

        if (!n){
            /* skip empty index */

            continue;
        }

        node_free(n);
    }

    free(m->nodes);
    free(m);
}
