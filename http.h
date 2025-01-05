#ifndef HTTP_H
#define HTTP_H

#include "log.h"
#include "map.h"

#include <json-c/json.h>

typedef struct http_client {
    logctx *log;
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
} http_response;

http_client *http_init(logctx *);

http_response *http_request(http_client *, http_method, const char *, const list *);

void http_response_free(http_response *);
void http_free(http_client *);

#endif
