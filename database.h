#ifndef DATABASE_H
#define DATABASE_H

#include "list.h"

#include <sqlite3.h>
#include <stdbool.h>

sqlite3 *database_init(const char *);

bool database_execute(sqlite3 *, const char *, const list *, list **, bool);

void database_free(sqlite3 *);

#endif
