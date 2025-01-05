#include "json_utils.h"

#include "list.h"
#include "log.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static logctx *logger = NULL;

list *json_array_to_list(json_object *value){
    if (!value || json_object_get_type(value) != json_type_array){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] json_array_to_list() - value is not a JSON array\n",
            __FILE__
        );

        return NULL;
    }

    list *l = list_init();

    if (!l){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] json_array_to_list() - list initialization failed\n",
            __FILE__
        );

        return NULL;
    }

    bool success = true;
    size_t length = json_object_array_length(value);

    for (size_t index = 0; index < length; ++index){
        json_object *itemobj = json_object_array_get_idx(value, index);
        json_type type = json_object_get_type(itemobj);

        list_item item = {0};

        if (type == json_type_array){
            list *tmp = json_array_to_list(itemobj);

            if (!tmp){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] json_array_to_list() - recursive call failed\n",
                    __FILE__
                );

                success = false;

                break;
            }

            item.type = L_TYPE_LIST;
            item.size = sizeof(*tmp);
            item.data = tmp;
        }
        else if (type == json_type_boolean){
            bool tmp = json_object_get_boolean(itemobj);

            item.type = L_TYPE_BOOL;
            item.size = sizeof(tmp);
            item.data_copy = &tmp;
        }
        else if (type == json_type_double){
            double tmp = json_object_get_double(itemobj);

            item.type = L_TYPE_DOUBLE;
            item.size = sizeof(tmp);
            item.data_copy = &tmp;
        }
        else if (type == json_type_int){
            int64_t tmp = json_object_get_int64(itemobj);

            item.type = L_TYPE_INT;
            item.size = sizeof(tmp);
            item.data_copy = &tmp;
        }
        else if (type == json_type_null){
            item.type = L_TYPE_NULL;
            item.size = 0;
            item.data = NULL;
        }
        else if (type == json_type_object){
            map *tmp = json_to_map(itemobj);

            if (!tmp){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] json_array_to_list() - json_to_map call failed\n",
                    __FILE__
                );

                success = false;

                break;
            }

            item.type = L_TYPE_MAP;
            item.size = sizeof(*tmp);
            item.data = tmp;
        }
        else if (type == json_type_string){
            const char *tmp = json_object_get_string(itemobj);

            item.type = L_TYPE_STRING;
            item.size = json_object_get_string_len(itemobj);
            item.data_copy = tmp;
        }
        else {
            log_write(
                logger,
                LOG_WARNING,
                "[%s] json_array_to_list() - unexpected json type %d\n",
                __FILE__,
                type
            );

            success = false;

            break;
        }

        success = list_append(l, &item);

        if (!success){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] json_array_to_list() - list_append call failed\n",
                __FILE__
            );

            break;
        }
    }

    if (!success){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] json_array_to_list() - failed to set list items\n",
            __FILE__
        );

        list_free(l);

        return NULL;
    }

    return l;
}

json_object *list_to_json_array(const list *l){
    if (!l){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] list_to_json_array() - list is NULL\n",
            __FILE__
        );

        return NULL;
    }

    json_object *array = json_object_new_array();

    if (!array){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] list_to_json_array() - array object intitialization failed\n",
            __FILE__
        );

        return NULL;
    }

    size_t length = list_get_length(l);

    for (size_t index = 0; index < length; ++index){
        ltype type = list_get_type(l, index);

        char tmpstr[2] = {0};
        json_object *obj = NULL;

        switch (type){
        case L_TYPE_BOOL:
            obj = json_object_new_boolean(list_get_bool(l, index));

            break;
        case L_TYPE_CHAR:
            tmpstr[0] = list_get_char(l, index);
            obj = json_object_new_string(tmpstr);

            break;
        case L_TYPE_DOUBLE:
            obj = json_object_new_double(list_get_double(l, index));

            break;
        case L_TYPE_INT:
        case L_TYPE_UINT:
            obj = json_object_new_int64(list_get_int(l, index));

            break;
        case L_TYPE_LIST:
            obj = list_to_json_array(list_get_list(l, index));

            break;
        case L_TYPE_MAP:
            log_write(
                logger,
                LOG_WARNING,
                "[%s] list_to_json_array() - map_to_json is not implemented yet\n",
                __FILE__
            );

            break;
        case L_TYPE_NULL:
            obj = NULL;

            break;
        case L_TYPE_STRING:
            obj = json_object_new_string(list_get_string(l, index));

            break;
        default:
            log_write(
                logger,
                LOG_WARNING,
                "[%s] list_to_json_array() - unsupported list type at index %ld\n",
                __FILE__,
                index
            );
        }

        if (type != L_TYPE_NULL && !obj){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] list_to_json_array() - failed to get json object\n",
                __FILE__
            );

            json_object_put(array);

            return NULL;
        }

        json_object_array_add(array, obj);
    }

    return array;
}

map *json_to_map(json_object *json){
    if (!json){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] json_to_map() - json is NULL\n",
            __FILE__
        );

        return NULL;
    }

    map *m = map_init();

    if (!m){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] json_to_map() - map initialization failed\n",
            __FILE__
        );

        return NULL;
    }

    struct json_object_iterator curr = json_object_iter_begin(json);
    struct json_object_iterator end = json_object_iter_end(json);

    while (!json_object_iter_equal(&curr, &end)){
        const char *key = json_object_iter_peek_name(&curr);
        json_object *valueobj = json_object_iter_peek_value(&curr);
        json_type type = json_object_get_type(valueobj);

        map_item k = {0};
        k.type = M_TYPE_STRING;
        k.size = strlen(key);
        k.data_copy = key;

        map_item v = {0};

        list *listvalue = NULL;
        bool boolvalue = false;
        double doublevalue = 0.0;
        int64_t intvalue = 0;
        const char *strvalue = NULL;
        map *mapvalue = NULL;

        switch (type){
        case json_type_array:
            listvalue = json_array_to_list(valueobj);

            if (!listvalue){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] json_to_map() - json_array_to_list call failed\n",
                    __FILE__
                );

                break;
            }

            v.type = M_TYPE_LIST;
            v.size = sizeof(*listvalue);
            v.data = listvalue;

            break;
        case json_type_boolean:
            boolvalue = json_object_get_boolean(valueobj);

            v.type = M_TYPE_BOOL;
            v.size = sizeof(boolvalue);
            v.data_copy = &boolvalue;

            break;
        case json_type_double:
            doublevalue = json_object_get_double(valueobj);

            v.type = M_TYPE_DOUBLE;
            v.size = sizeof(doublevalue);
            v.data_copy = &doublevalue;

            break;
        case json_type_int:
            intvalue = json_object_get_int64(valueobj);

            v.type = M_TYPE_INT;
            v.size = sizeof(intvalue);
            v.data_copy = &intvalue;

            break;
        case json_type_null:
            strvalue = NULL;

            v.type = M_TYPE_NULL;
            v.size = sizeof(strvalue);
            v.data_copy = &strvalue;

            break;
        case json_type_object:
            mapvalue = json_to_map(valueobj);

            if (!mapvalue){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] json_to_map() - recursive call failed\n",
                    __FILE__
                );

                break;
            }

            v.type = M_TYPE_MAP;
            v.size = sizeof(*mapvalue);
            v.data = mapvalue;

            break;
        case json_type_string:
            strvalue = json_object_get_string(valueobj);

            v.type = M_TYPE_STRING;
            v.size = strlen(strvalue);
            v.data_copy = strvalue;

            break;
        default:
            log_write(
                logger,
                LOG_ERROR,
                "[%s] json_to_map() - unexpected json type - %d\n",
                __FILE__,
                type
            );
        }

        if (!map_set(m, &k, &v)){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] json_to_map() - failed to add key: %s\n",
                __FILE__,
                key
            );

            if (listvalue){
                list_free(listvalue);
            }
            else if (mapvalue){
                map_free(mapvalue);
            }

            map_free(m);

            return NULL;
        }

        json_object_iter_next(&curr);
    }

    return m;
}

json_object *map_to_json(const map *m){
    if (!m){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] map_to_json() - map is NULL\n",
            __FILE__
        );

        return NULL;
    }

    json_object *json = json_object_new_object();

    if (!json){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] map_to_json() - json object initialization failed\n",
            __FILE__
        );

        return NULL;
    }

    /* iterate through map and stack in json object --- BOOKMARK --- */

    return json;
}

bool json_merge_objects(json_object *from, json_object *into){
    if (!from || !into){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] json_merge_objects() - objects are NULL\n",
            __FILE__
        );

        return false;
    }
    else if (json_object_equal(from, into)){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] json_merge_objects() - objects are equal\n",
            __FILE__
        );

        return true;
    }

    struct json_object_iterator curr = json_object_iter_begin(from);
    struct json_object_iterator end = json_object_iter_end(from);

    while (!json_object_iter_equal(&curr, &end)){
        const char *key = json_object_iter_peek_name(&curr);
        json_object *valueobj = json_object_iter_peek_value(&curr);
        json_object *copyobj = json_object_get(valueobj);

        if (json_object_object_add(into, key, copyobj)){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] json_merge_objects() - json_object_object_add call failed\n",
                __FILE__
            );

            json_object_put(copyobj);

            return false;
        }

        json_object_iter_next(&curr);
    }

    return true;
}
