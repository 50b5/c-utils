#include "log.h"

#include "str.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

logctx *log_init(const char *filename, FILE *stream){
    logctx *log = malloc(sizeof(*log));

    if (!log){
        DLOG(
            "[%s] log_init() - log alloc failed\n",
            __FILE__
        );

        return NULL;
    }

    if (filename){
        log->handle = fopen(filename, "a+");

        if (!log->handle){
            DLOG(
                "[%s] log_init() - failed to open '%s' in append mode: %s\n",
                __FILE__,
                filename,
                strerror(errno)
            );

            free(log);

            return NULL;
        }

        log->file = true;
    }
    else {
        log->handle = stream ? stream : LOG_DEFAULT_STREAM;
        log->file = false;
    }

    return log;
}

bool log_write(const logctx *log, logtype type, const char *format, ...){
    if (!format){
        DLOG(
            "[%s] log_write() - format is NULL\n",
            __FILE__
        );

        return false;
    }

    FILE *handle = LOG_DEFAULT_STREAM;

    if (log && log->handle){
        handle = log->handle;
    }

    char timestamp[LOG_TIMESTAMP_LENGTH];

    if (!string_from_time(0, true, LOG_TIMESTAMP_FORMAT, timestamp, sizeof(timestamp))){
        DLOG(
            "[%s] log_write() - failed to get timestamp string: %s\n",
            __FILE__,
            strerror(errno)
        );
    }

    const char *typestr = NULL;

    switch (type){
    case LOG_DEBUG:
        typestr = "DEBUG";

        break;
    case LOG_ERROR:
        typestr = "ERROR";

        break;
    case LOG_WARNING:
        typestr = "WARN";

        break;
    case LOG_RAW:
        /* keep typestr NULL */

        break;
    default:
        DLOG(
            "[%s] log_write() - unexpected type\n",
            __FILE__
        );
    }

    if (typestr){
        fprintf(handle, "(%s) %s ", timestamp, typestr);
    }
    else if (type != LOG_RAW) {
        fprintf(handle, "(%s) ", timestamp);
    }

    va_list args;

    va_start(args, format);

    int err = vfprintf(handle, format, args);

    va_end(args);

    if (err < 0){
        DLOG(
            "[%s] log_write() - vfprintf call failed\n",
            __FILE__
        );

        return false;
    }

    return true;
}

void log_free(logctx *log){
    if (!log){
        DLOG(
            "[%s] log_free() - log is NULL\n",
            __FILE__
        );

        return;
    }

    if (log->file){
        fflush(log->handle);
        fclose(log->handle);
    }

    free(log);
}
