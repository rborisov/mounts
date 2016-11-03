#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <syslog.h>
#include <time.h>

#include "config.h"
#include "db_utils.h"
#include "sql.h"

sqlite3 *conn;
sqlite3_stmt *res;
char *sqlchar0, *sqlchar1, *sqlchar2, *sqlchar3;

void convert_str(char *instr)
{
    char *out = instr;

    if (!instr)
        return;

    while (*out) {
        if (*out == '\'')
            *out = '\"';
        out++;
    }
    *out = '\0';
}

void convert_str_back(char *instr)
{
    char *out = instr;

    if (!instr)
        return;

    while (*out) {
        if (*out == '\"')
            *out = '\'';
        out++;
    }
    *out = '\0';
}

int db_init()
{
    int rc;
    char dbpath[128];
    sprintf(dbpath, "%s/rcardb.sqlite", _DB_PATH_);

    rc = sqlite3_open(dbpath, &conn);
    if (rc != SQLITE_OK) {
        syslog(LOG_DEBUG, "%s, %s\n", __func__, sqlite3_errmsg(conn));
        sqlite3_close(conn);
    }
    return rc;
}

/*
 * return: album or NULL
 * note: sqlite3_free(result) is required
 */
char* db_get_song_album(char* song, char* artist)
{
    convert_str(song);
    convert_str(artist);

    sqlite3_free(sqlchar0);
    sqlchar0 = sql_get_text_field(conn, "SELECT album FROM Songs WHERE "
            "song = '%s' AND artist = '%s'", song, artist);
    return sqlchar0;
}

char *db_get_song_name(int id)
{
    sqlite3_free(sqlchar0);
    sqlchar0 = sql_get_text_field(conn, "SELECT song FROM Songs WHERE id = '%i'", id);
    convert_str_back(sqlchar0);
    return sqlchar0;
}

char *db_get_song_artist(int id)
{
    sqlite3_free(sqlchar1);
    sqlchar1 = sql_get_text_field(conn, "SELECT artist FROM Songs WHERE id = '%i'", id);
    convert_str_back(sqlchar1);
    return sqlchar1;
}

int db_get_prior_song_by_rating_first()
{
    int rc;
    rc = sqlite3_prepare_v2(conn, "SELECT id FROM Songs ORDER by rating ASC", -1, &res, 0);

    if (rc == SQLITE_OK) {
        if (sqlite3_step(res) == SQLITE_ROW) {
            return sqlite3_column_int(res, 0);
        }
    }
    sqlite3_finalize(res);

    return 0;
}

int db_get_song_by_rating_next()
{
    if (sqlite3_step(res) == SQLITE_ROW) {
        return sqlite3_column_int(res, 0);
    }
    sqlite3_finalize(res);

    return 0;
}

void db_close()
{
    sqlite3_free(sqlchar0);
    sqlite3_free(sqlchar1);
    sqlite3_free(sqlchar2);
    sqlite3_close(conn);
}
