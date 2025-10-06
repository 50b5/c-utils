#ifndef STR_H
#define STR_H

#include "list.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

char *string_create(const char *, ...);
bool string_copy(const char *, char *, size_t);
char *string_duplicate(const char *);

list *string_split_len(const char *, size_t, const char *, long);
list *string_split(const char *, const char *, long);
char *string_join(const list *, const char *);

char *string_lower(char *);
char *string_upper(char *);

bool string_from_time(time_t, bool, const char *, char *, size_t);

bool string_to_int(const char *, int *, int);
bool string_is_numeric(const char *);

#endif
