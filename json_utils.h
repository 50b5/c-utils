#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "list.h"
#include "map.h"

#include <json-c/json.h>

list *json_array_to_list(json_object *);
json_object *list_to_json_array(const list *);

map *json_to_map(json_object *);
json_object *map_to_json(const map *);

bool json_merge_objects(json_object *, json_object *);

#endif
