#ifndef HTTP_H
#define HTTP_H

#include "log.h"
#include "map.h"

#include <json-c/json.h>

#define HTTP_DEFAULT_USER_AGENT "c-utils/1.0 (https://github.com/50b5/c-utils)"

typedef struct http_client {
    const logctx *log;
} http_client;

typedef enum http_method {
    HTTP_GET,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_POST,
    HTTP_PUT
} http_method;

typedef struct http_response {
    long status;
    map *headers;
    json_object *data;
    char *raw_data;
} http_response;

http_client *http_init(const logctx *);

http_response *http_request(http_client *, http_method, const char *, const list *);

void http_response_free(http_response *);
void http_free(http_client *);

#endif
