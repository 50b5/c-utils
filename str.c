#define _POSIX_C_SOURCE 200809L

#include "str.h"

#include "log.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static logctx *logger = NULL;

char *string_create(const char *format, ...){
    if (!format){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_create() - format is NULL\n",
            __FILE__
        );

        return NULL;
    }

    va_list args;
    va_list argscpy;

    va_start(argscpy, format);

    int stringlen = vsnprintf(NULL, 0, format, argscpy);

    va_end(argscpy);

    if (stringlen < 0){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_create() - vsnprintf call failed for length\n",
            __FILE__
        );

        return NULL;
    }

    char *string = malloc(++stringlen);

    if (!string){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_create() - string alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    va_start(args, format);

    stringlen = vsnprintf(string, stringlen, format, args);

    va_end(args);

    if (stringlen < 0){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_create() - vsnprintf call failed for string\n",
            __FILE__
        );

        free(string);

        return NULL;
    }

    return string;
}

bool string_copy(const char *input, char *output, size_t outputsize){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_copy() - input is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!output){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_copy() - output is NULL\n",
            __FILE__
        );

        return false;
    }

    size_t inputlen = strlen(input);

    if (inputlen < outputsize){
        outputsize = inputlen;
    }

    memcpy(output, input, outputsize);

    output[outputsize] = '\0';

    return true;
}

char *string_duplicate(const char *input){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_duplicate() - input is NULL\n",
            __FILE__
        );

        return NULL;
    }

    size_t inputlen = strlen(input);
    char *output = malloc(inputlen + 1);

    if (!output){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_duplicate() - output alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    memcpy(output, input, inputlen);

    output[inputlen] = '\0';

    return output;
}

list *string_split_len(const char *input, size_t inputlen, const char *delim, long count){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_split_len() - input is NULL\n",
            __FILE__
        );

        return NULL;
    }
    else if (!delim){
        delim = " ";
    }

    list *tokens = list_init();

    if (!tokens){
        return NULL;
    }

    size_t delimlen = strlen(delim);

    if (count == 0 || delimlen == 0){
        list_item item = {0};
        item.type = L_TYPE_STRING;
        item.size = inputlen;
        item.data_copy = input;

        if (!list_append(tokens, &item)){
            list_free(tokens);

            return NULL;
        }

        return tokens;
    }

    long delimcount = 0;
    size_t ptrindex = 0;
    const char *ptr = NULL;

    while ((ptr = strstr(&input[ptrindex], delim))){
        size_t tokenlen = ptr - &input[ptrindex];

        list_item item = {0};
        item.type = L_TYPE_STRING;
        item.size = tokenlen;
        item.data_copy = input + ptrindex;

        if (!list_append(tokens, &item)){
            list_free(tokens);

            return NULL;
        }

        ptrindex += tokenlen + delimlen;

        if (count > 0 && delimcount++ == count){
            break;
        }
    }

    if (ptrindex <= inputlen){
        size_t tokenlen = inputlen - ptrindex;

        list_item item = {0};
        item.type = L_TYPE_STRING;
        item.size = tokenlen;
        item.data_copy = input + ptrindex;

        if (!list_append(tokens, &item)){
            list_free(tokens);

            return NULL;
        }
    }

    return tokens;
}

list *string_split(const char *input, const char *delim, long count){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_split() - input is NULL\n",
            __FILE__
        );

        return NULL;
    }
    else if (!delim){
        delim = " ";
    }

    return string_split_len(input, strlen(input), delim, count);
}

char *string_join(const list *input, const char *delim){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_join() - input is NULL\n",
            __FILE__
        );

        return NULL;
    }
    else if (!delim){
        delim = " ";
    }

    size_t delimlen = strlen(delim);

    size_t outputlen = 0;
    size_t inputlen = list_get_length(input);

    for (size_t index = 0; index < inputlen; ++index){
        if ((index + 1) < inputlen){
            outputlen += delimlen;
        }

        outputlen += list_get_item_size(input, index);
    }

    char *output = malloc(outputlen + 1);

    if (!output){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_join() - output alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    size_t currlen = 0;

    for (size_t index = 0; index < inputlen; ++index){
        const char *ostr = list_get_string(input, index);
        size_t ostrlen = list_get_item_size(input, index);

        memcpy(&output[currlen], ostr, ostrlen);

        currlen += ostrlen;

        if ((index + 1) < inputlen){
            memcpy(&output[currlen], delim, delimlen);

            currlen += delimlen;
        }
    }

    output[currlen] = '\0';

    return output;
}

char *string_lower(char *input){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_lower() - input is NULL\n",
            __FILE__
        );

        return NULL;
    }

    char *copy = input;

    for (; *copy; copy++){
        *copy = tolower(*copy);
    }

    return input;
}

char *string_upper(char *input){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_upper() - input is NULL\n",
            __FILE__
        );

        return NULL;
    }

    char *copy = input;

    for (; *copy; copy++){
        *copy = toupper(*copy);
    }

    return input;
}

bool string_from_time(time_t timet, bool local, const char *format, char *output, size_t outputsize){
    if (!format){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_from_time() - format is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!output){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_from_time() - output is NULL\n",
            __FILE__
        );

        return false;
    }

    if (timet == 0){
        timet = time(NULL);
    }

    struct tm *timetm = local ? localtime(&timet) : gmtime(&timet);

    if (!timetm){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_from_time() - localtime call failed\n",
            __FILE__
        );

        return false;
    }

    if (!strftime(output, outputsize, format, timetm)){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] string_from_time() - strftime call failed\n",
            __FILE__
        );

        return false;
    }

    return true;
}

bool string_to_int(const char *input, int *output, int base){
    if (!input){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_to_int() - input is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!output){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_to_int() - output is NULL\n",
            __FILE__
        );

        return false;
    }

    char *end;

    errno = 0;
    int value = strtol(input, &end, base);

    if (errno == ERANGE || (*end != '\0' || *input == '\0')){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] string_to_int() - %s conversion to base %d failed\n",
            __FILE__,
            input,
            base
        );

        return false;
    }

    *output = value;

    return true;
}
