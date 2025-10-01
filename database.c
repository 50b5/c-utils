#include "database.h"

#include "log.h"

#include <stdio.h>
#include <string.h>

static logctx *logger = NULL;

static bool append_rows_named(sqlite3 *db, sqlite3_stmt *stmt, list *res){
    int err = SQLITE_ROW;

    do {
        map *row = map_init();

        if (!row){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] append_rows() - row initialization failed\n",
                __FILE__
            );

            return false;
        }

        size_t columns = sqlite3_column_count(stmt);

        for (size_t index = 0; index < columns; index++){
            if (err != SQLITE_DONE && err != SQLITE_ROW){
                log_write(
                    logger,
                    LOG_WARNING,
                    "[%s] append_rows() - sqlite3_step failed: %s\n",
                    __FILE__,
                    sqlite3_errmsg(db)
                );

                map_free(row);

                return false;
            }

            int ctype = sqlite3_column_type(stmt, index);
            const char *cname = sqlite3_column_name(stmt, index);

            map_item k = {0};
            k.type = M_TYPE_STRING;
            k.size = strlen(cname);
            k.data_copy = cname;

            map_item v = {0};

            if (ctype == SQLITE_FLOAT){
                double value = sqlite3_column_double(stmt, index);

                v.type = M_TYPE_DOUBLE;
                v.size = sizeof(value);
                v.data_copy = &value;
            }
            else if (ctype == SQLITE_INTEGER){
                int64_t value = sqlite3_column_int64(stmt, index);

                v.type = M_TYPE_INT;
                v.size = sizeof(value);
                v.data_copy = &value;
            }
            else if (ctype == SQLITE_NULL){
                void *value = NULL;

                v.type = M_TYPE_NULL;
                v.size = sizeof(value);
                v.data = value;
            }
            else {
                const void *value = sqlite3_column_blob(stmt, index);

                v.type = M_TYPE_STRING;
                v.size = sqlite3_column_bytes(stmt, index);
                v.data_copy = value;
            }

            if (!map_set(row, &k, &v)){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] append_rows_named() - map_set call failed\n",
                    __FILE__
                );

                map_free(row);

                return false;
            }
        }

        list_item item = {0};
        item.type = L_TYPE_MAP;
        item.size = sizeof(*row);
        item.data = row;

        if (!list_append(res, &item)){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] append_rows_named() - list_append call failed\n",
                __FILE__
            );

            map_free(row);

            return false;
        }
    } while ((err = sqlite3_step(stmt)) == SQLITE_ROW);

    return true;
}

static bool append_rows(sqlite3 *db, sqlite3_stmt *stmt, list *res){
    int err = SQLITE_ROW;

    do {
        list *row = list_init();

        if (!row){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] append_rows() - row initialization failed\n",
                __FILE__
            );

            return false;
        }

        size_t columns = sqlite3_column_count(stmt);

        for (size_t index = 0; index < columns; index++){
            if (err != SQLITE_DONE && err != SQLITE_ROW){
                log_write(
                    logger,
                    LOG_WARNING,
                    "[%s] append_rows() - sqlite3_step call failed: %s\n",
                    __FILE__,
                    sqlite3_errmsg(db)
                );

                list_free(row);

                return false;
            }

            int ctype = sqlite3_column_type(stmt, index);

            list_item item = {0};

            if (ctype == SQLITE_FLOAT){
                double value = sqlite3_column_double(stmt, index);

                item.type = L_TYPE_DOUBLE;
                item.size = sizeof(value);
                item.data_copy = &value;
            }
            else if (ctype == SQLITE_INTEGER){
                int64_t value = sqlite3_column_int64(stmt, index);

                item.type = L_TYPE_INT;
                item.size = sizeof(value);
                item.data_copy = &value;
            }
            else if (ctype == SQLITE_NULL){
                void *value = NULL;

                item.type = L_TYPE_NULL;
                item.size = sizeof(value);
                item.data_copy = value;
            }
            else {
                const void *value = sqlite3_column_blob(stmt, index);
                size_t valuelen = sqlite3_column_bytes(stmt, index);

                item.type = L_TYPE_STRING;
                item.size = valuelen;
                item.data_copy = value;
            }

            if (!list_append(row, &item)){
                log_write(
                    logger,
                    LOG_ERROR,
                    "[%s] append_rows() - list_append call failed\n",
                    __FILE__
                );

                list_free(row);

                return false;
            }
        }

        list_item item = {0};
        item.type = L_TYPE_LIST;
        item.size = sizeof(*row);
        item.data = row;

        if (!list_append(res, &item)){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] append_rows() - list_append call failed\n",
                __FILE__
            );

            list_free(row);

            return false;
        }
    } while ((err = sqlite3_step(stmt)) == SQLITE_ROW);

    return true;
}

sqlite3 *database_init(const char *path){
    sqlite3 *db = NULL;

    int err = 0;

    if (path){
        err = sqlite3_open_v2(
            path,
            &db,
            SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
            NULL
        );
    }
    else {
        err = sqlite3_open_v2(
            path,
            &db,
            SQLITE_OPEN_MEMORY,
            NULL
        );
    }

    if (err != SQLITE_OK){
        log_write(
            logger,
            LOG_ERROR,
            "[%s] database_init() - sqlite3_open_v2 call failed: %s\n",
            __FILE__,
            sqlite3_errmsg(db)
        );

        database_free(db);

        return NULL;
    }

    return db;
}

bool database_execute(sqlite3 *db, const char *sql, const list *params, list **res, bool named){
    if (!db){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] database_execute() - database is NULL\n",
            __FILE__
        );

        return false;
    }
    else if (!sql){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] database_execute() - query is NULL\n",
            __FILE__
        );

        return false;
    }

    sqlite3_stmt *stmt;
    int err = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (err != SQLITE_OK){
        log_write(
            logger,
            LOG_WARNING,
            "[%s] database_execute() - sqlite3_prepare_v2 call failed: %s\n",
            __FILE__,
            sqlite3_errmsg(db)
        );

        return false;
    }

    if (params){
        size_t paramslen = list_get_length(params);

        for (size_t index = 0; index < paramslen; ++index){
            ltype type = list_get_type(params, index);

            switch (type){
                case L_TYPE_BOOL:
                    err = sqlite3_bind_int(
                        stmt,
                        index + 1,
                        list_get_bool(params, index) ? 1 : 0
                    );

                    break;
                case L_TYPE_CHAR:
                    err = sqlite3_bind_text(
                        stmt,
                        index + 1,
                        (char []){list_get_char(params, index), '\0'},
                        2,
                        NULL
                    );

                    break;
                case L_TYPE_DOUBLE:
                    err = sqlite3_bind_double(
                        stmt,
                        index + 1,
                        list_get_double(params, index)
                    );

                    break;
                case L_TYPE_INT:
                    err = sqlite3_bind_int64(
                        stmt,
                        index + 1,
                        list_get_int(params, index)
                    );

                    break;
                case L_TYPE_NULL:
                    err = sqlite3_bind_null(stmt, index + 1);

                    break;
                case L_TYPE_STRING:
                    err = sqlite3_bind_text(
                        stmt,
                        index + 1,
                        list_get_string(params, index),
                        -1,
                        NULL
                    );

                    break;
                default:
                    log_write(
                        logger,
                        LOG_WARNING,
                        "[%s] database_execute() - unhandled node type %d\n",
                        __FILE__,
                        type
                    );

                    sqlite3_finalize(stmt);

                    return false;
            }

            if (err != SQLITE_OK){
                log_write(
                    logger,
                    LOG_WARNING,
                    "[%s] database_execute() - binding failed: %s\n",
                    __FILE__,
                    sqlite3_errmsg(db)
                );

                sqlite3_finalize(stmt);

                return false;
            }
        }
    }

    bool success = false;

    err = sqlite3_step(stmt);

    if (res && err == SQLITE_ROW){
        list *rescopy = list_init();

        if (!rescopy){
            log_write(
                logger,
                LOG_ERROR,
                "[%s] database_execute() - copy list initialization failed\n",
                __FILE__
            );

            sqlite3_finalize(stmt);

            return false;
        }

        if (named){
            success = append_rows_named(db, stmt, rescopy);
        }
        else {
            success = append_rows(db, stmt, rescopy);
        }

        if (!success){
            list_free(rescopy);

            rescopy = NULL;
        }

        *res = rescopy;
    }
    else if (err == SQLITE_DONE || err == SQLITE_ROW){
        success = true;
    }
    else {
        log_write(
            logger,
            LOG_WARNING,
            "[%s] database_execute() - sqlite3_step call failed: %s\n",
            __FILE__,
            sqlite3_errmsg(db)
        );
    }

    sqlite3_finalize(stmt);

    return success;
}

void database_free(sqlite3 *db){
    if (!db){
        log_write(
            logger,
            LOG_DEBUG,
            "[%s] database_free() - database is NULL\n",
            __FILE__
        );

        return;
    }

    sqlite3_close_v2(db);
}
