#include "http.h"

#include "str.h"

#include <stdlib.h>
#include <time.h>

#include <curl/curl.h>

struct responsestr {
    char *data;
    size_t size;
};

static const logctx *logger = NULL;

static size_t write_response_headers(char *data, size_t size, size_t nitems, void *out){
    size *= nitems;
    map *m = out;

    list *parts = string_split_len(data, size, ": ", 1);

    if (!parts){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] write_response_headers() - string_split_len call failed\n",
            __FILE__
        );

        return 0;
    }
    else if (list_get_length(parts) != 2){
        list_free(parts);

        return size;
    }

    const char *keystr = list_get_string(parts, 0);
    const char *valuestr = list_get_string(parts, 1);

    map_item key = {0};
    key.type = M_TYPE_STRING;
    key.size = strlen(keystr);
    key.data_copy = keystr;

    map_item value = {0};
    value.type = M_TYPE_STRING;
    value.size = strlen(valuestr) - 2;
    value.data_copy = valuestr;

    bool success = map_set(m, &key, &value);

    list_free(parts);

    if (!success){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] write_response_headers() - map_set call failed\n",
            __FILE__
        );

        return 0;
    }

    return size;
}

static size_t write_response_data(char *data, size_t size, size_t nmemb, void *out){
    size *= nmemb;
    struct responsestr *res = out;

    char *ptr = realloc(res->data, res->size + size + 1);

    if (!ptr){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] write_response_data() - failed to realloc response pointer\n",
            __FILE__
        );

        return 0;
    }

    memcpy(ptr + res->size, data, size);

    res->data = ptr;
    res->size += size;
    res->data[res->size] = '\0';

    return size;
}

static struct curl_slist *create_request_header_list(const list *headers){
    struct curl_slist *reqheaders = NULL;
    struct curl_slist *tmp = NULL;

    if (!headers){
        return reqheaders;
    }

    for (size_t index = 0; index < list_get_length(headers); ++index){
        const char *header = list_get_string(headers, index);
        tmp = curl_slist_append(reqheaders, header);

        if (!tmp){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] create_request_header_list() - failed to append header\n",
                __FILE__
            );

            curl_slist_free_all(reqheaders);

            return NULL;
        }

        reqheaders = tmp;
    }

    return reqheaders;
}

static bool set_request_method(CURL *handle, http_method method){
    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_method() - handle is NULL\n",
            __FILE__
        );

        return false;
    }

    CURLcode err = CURLE_FAILED_INIT;

    switch (method){
    case HTTP_GET:
        err = curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);

        if (err != CURLE_OK){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] set_request_method() - failed to set CURLOPT_HTTPGET\n",
                __FILE__
            );

            return false;
        }

        break;
    case HTTP_DELETE:
        err = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");

        if (err != CURLE_OK){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] set_request_method() - failed to set CURLOPT_CUSTOMREQUEST (DELETE)\n",
                __FILE__
            );

            return false;
        }

        break;
    case HTTP_PATCH:
        err = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PATCH");

        if (err != CURLE_OK){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] set_request_method() - failed to set CURLOPT_CUSTOMREQUEST (PATCH)\n",
                __FILE__
            );

            return false;
        }

        break;
    case HTTP_POST:
        err = curl_easy_setopt(handle, CURLOPT_POST, 1);

        if (err != CURLE_OK){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] set_request_method() - failed to set CURLOPT_POST\n",
                __FILE__
            );

            return false;
        }

        break;
    case HTTP_PUT:
        err = curl_easy_setopt(handle, CURLOPT_UPLOAD, 1);

        if (err != CURLE_OK){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] set_request_method() - failed to set CURLOPT_PUT\n",
                __FILE__
            );

            return false;
        }

        break;
    default:
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_method() - unimplemented request method\n",
            __FILE__
        );

        return false;
    }

    return true;
}

static bool set_request_url(CURL *handle, const char *url){
    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_url() - handle is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!url){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_url() - url is NULL\n",
            __FILE__);

        return false;
    }

    CURLcode err = curl_easy_setopt(handle, CURLOPT_URL, url);

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_url() - failed to set CURLOPT_URL\n",
            __FILE__
        );

        return false;
    }

    return true;
}

static bool set_request_headers(CURL *handle, const struct curl_slist *headers){
    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_headers() - handle is NULL\n",
            __FILE__
        );

        return false;
    }

    CURLcode err = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_request_headers() - failed to set CURLOPT_HTTPHEADER\n",
            __FILE__
        );

        return false;
    }

    return true;
}

static bool set_response_header_writer(CURL *handle, void *out){
    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_header_writer() - handle is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!out){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_header_writer() - out is NULL\n",
            __FILE__
        );

        return false;
    }

    CURLcode err = curl_easy_setopt(
        handle,
        CURLOPT_HEADERFUNCTION,
        write_response_headers
    );

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_header_writer() - failed to set CURLOPT_HEADERFUNCTION\n",
            __FILE__
        );

        return false;
    }

    err = curl_easy_setopt(handle, CURLOPT_HEADERDATA, out);

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_header_writer() - failed to set CURLOPT_HEADERDATA\n",
            __FILE__
        );

        return false;
    }

    return true;
}

static bool set_response_data_writer(CURL *handle, void *out){
    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_data_writer() - handle is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!out){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_data_writer() - out is NULL\n",
            __FILE__
        );

        return false;
    }

    CURLcode err = curl_easy_setopt(
        handle,
        CURLOPT_WRITEFUNCTION,
        write_response_data
    );

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_data_writer() - failed to set CURLOPT_WRITEFUNCTION\n",
            __FILE__
        );

        return false;
    }

    err = curl_easy_setopt(handle, CURLOPT_WRITEDATA, out);

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] set_response_data_writer() - failed to set CURLOPT_WRITEDATA\n",
            __FILE__
        );

        return false;
    }

    return true;
}

static http_response *create_response(CURL *handle, map *headers){
    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] create_response() - handle is NULL\n",
            __FILE__
        );

        return NULL;
    }

    http_response *response = calloc(1, sizeof(*response));

    if (!response){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] create_response() - calloc for response failed\n",
            __FILE__
        );

        return NULL;
    }

    CURLcode err = curl_easy_getinfo(
        handle,
        CURLINFO_RESPONSE_CODE,
        &response->status
    );

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] create_response() - failed to get CURLINFO_RESPONSE_CODE\n",
            __FILE__
        );

        free(response);

        return NULL;
    }

    response->headers = headers;

    return response;
}

static void handle_response_status(const http_response *response){
    if (!response){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] handle_response_status() - response is NULL\n",
            __FILE__
        );

        return;
    }

    double retryafter = 0;
    json_object *obj = NULL;

    switch (response->status){
    case 301:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) resource moved permanently\n",
            __FILE__,
            response->status
        );

        break;
    case 304:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) resource was not modified\n",
            __FILE__,
            response->status
        );

        break;
    case 400:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) sent invalid request\n",
            __FILE__,
            response->status
        );

        break;
    case 401:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) sent unauthorized request\n",
            __FILE__,
            response->status
        );

        break;
    case 403:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) requested privileged resource\n",
            __FILE__,
            response->status
        );

        break;
    case 404:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) requested nonexistent resource\n",
            __FILE__,
            response->status
        );

        break;
    case 405:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) invalid method for requested resource\n",
            __FILE__,
            response->status
        );

        break;
    case 429:
        obj = json_object_object_get(response->data, "retry_after");

        if (!obj){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] handle_response_status() - failed to get retry_after json object\n",
                __FILE__
            );

            break;
        }

        retryafter = json_object_get_double(obj);

        log_write(
            logger,
            LOG_WARNING,
            "[%s] handle_response_status() - rate limited: retry after %d\n",
            __FILE__,
            retryafter
        );

        break;
    default:
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] handle_response_status() - (%d) unexpected status code\n",
            __FILE__,
            response->status
        );
    }
}

http_client *http_init(const logctx *lhandle){
    logger = lhandle;

    if (curl_global_init(CURL_GLOBAL_ALL)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_init() - curl_global_init call failed\n",
            __FILE__
        );

        return NULL;
    }

    http_client *http = calloc(1, sizeof(*http));

    if (!http){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_init() - alloc for http_client failed\n",
            __FILE__
        );

        curl_global_cleanup();

        return NULL;
    }

    http->log = lhandle;

    return http;
}

http_response *http_request(http_client *http, http_method method, const char *path, const list *headers){
    if (!http){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - http is NULL\n",
            __FILE__
        );

        return NULL;
    }
    else if (!path){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - path is NULL\n",
            __FILE__
        );

        return NULL;
    }

    CURL *handle = curl_easy_init();

    if (!handle){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - failed to create handle\n"
            __FILE__
        );

        return NULL;
    }

    /* debug */
    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);

    if (!set_request_method(handle, method)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - set_request_method call failed\n",
            __FILE__
        );

        curl_easy_cleanup(handle);

        return NULL;
    }

    if (!set_request_url(handle, path)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - set_request_url call failed\n",
            __FILE__
        );

        curl_easy_cleanup(handle);

        return NULL;
    }

    CURLcode err = curl_easy_setopt(
        handle,
        CURLOPT_USERAGENT,
        HTTP_DEFAULT_USER_AGENT
    );

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - failed to set CURLOPT_USERAGENT\n",
            __FILE__
        );

        curl_easy_cleanup(handle);

        return NULL;
    }

    struct curl_slist *requestheaders = create_request_header_list(headers);

    if (headers && !requestheaders){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - create_request_header_list call failed\n",
            __FILE__
        );

        curl_easy_cleanup(handle);

        return NULL;
    }

    if (!set_request_headers(handle, requestheaders)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - set_request_headers call failed\n",
            __FILE__
        );

        curl_slist_free_all(requestheaders);
        curl_easy_cleanup(handle);

        return NULL;
    }

    struct responsestr out = {0};

    if (!set_response_data_writer(handle, &out)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - set_response_data_writer call failed\n",
            __FILE__
        );

        curl_slist_free_all(requestheaders);
        curl_easy_cleanup(handle);

        return NULL;
    }

    map *responseheaders = map_init();

    if (!responseheaders){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - map_init call failed\n",
            __FILE__
        );

        curl_slist_free_all(requestheaders);
        curl_easy_cleanup(handle);

        return NULL;
    }

    if (!set_response_header_writer(handle, responseheaders)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - set_response_header_writer call failed\n",
            __FILE__
        );

        map_free(responseheaders);
        curl_slist_free_all(requestheaders);
        curl_easy_cleanup(handle);

        return NULL;
    }

    err = curl_easy_perform(handle);

    curl_slist_free_all(requestheaders);

    if (err != CURLE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - failed to perform request\n",
            __FILE__
        );

        map_free(responseheaders);
        curl_easy_cleanup(handle);

        return NULL;
    }

    http_response *response = create_response(handle, responseheaders);

    curl_easy_cleanup(handle);

    if (!response){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] http_request() - create_response call failed\n",
            __FILE__
        );

        map_free(responseheaders);
    }

    if (out.data){
        response->raw_data = out.data;
        response->data = json_tokener_parse(response->raw_data);

        if (!response->data){
            log_write(
                logger,
                LOG_WARNING,
                "[%s] http_request() - json_tokener_parse call failed on `%s`\n",
                __FILE__,
                response->raw_data
            );
        }
    }
    else {
        response->raw_data = NULL;
        response->data = NULL;
    }

    handle_response_status(response);

    return response;
}

void http_response_free(http_response *response){
    if (!response){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] http_response_free() - response is NULL\n",
            __FILE__
        );

        return;
    }

    json_object_put(response->data);
    map_free(response->headers);
    free(response->raw_data);
    free(response);
}

void http_free(http_client *http){
    if (!http){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] http_free() - http is NULL\n",
            __FILE__
        );

        return;
    }

    free(http);

    curl_global_cleanup();
}
