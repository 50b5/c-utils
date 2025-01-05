#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdio.h>

#define LOG_DEFAULT_STREAM stderr
#define LOG_TIMESTAMP_FORMAT "%m/%d/%Y %H:%M:%S"
#define LOG_TIMESTAMP_LENGTH 32

#ifdef DEBUG
#define DLOG(...) fprintf(LOG_DEFAULT_STREAM, __VA_ARGS__)
#else
#define DLOG(...)
#endif

typedef enum logtype {
    LOG_DEBUG,
    LOG_WARNING,
    LOG_ERROR,
    LOG_RAW
} logtype;

typedef struct logctx {
    FILE *handle;
    bool file;
} logctx;

logctx *log_init(const char *, FILE *);

bool log_write(const logctx *, logtype, const char *, ...);

void log_free(logctx *);

#endif
