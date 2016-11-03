#include <stdio.h>
#include <unistd.h>
#include <mpd/client.h>
#include "mpd-utils.h"
#include "config.h"

struct mpd_connection * setup_connection(void)
{
    struct mpd_connection *conn = mpd_connection_new(NULL, NULL, 0);
    if (conn == NULL) {
        printf("%s: Out of memory\n", __func__);
        return NULL;
    }

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        printf("%s: mpd connection error: %s\n", __func__, mpd_connection_get_error_message(conn));
        return NULL;
    }

    return conn;
}

int update_mpd_db(char *song)
{
    struct mpd_connection *conn = setup_connection();
    if (conn) {
        mpd_run_update(conn, song);
        mpd_connection_free(conn);
        return 0;
    }
    return -1;
}

int isin_mpd_db(char *artist, char *title)
{
    struct mpd_song *song;
    int cnt = 0;

    struct mpd_connection *conn = setup_connection();

    if (conn) {
        if(mpd_search_db_songs(conn, false) == false) {
            printf("%s: mpd_search_db_songs failed\n", __func__);
            goto search_done;
        } else if(mpd_search_add_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
                    0, artist) == false) {
            printf("%s: mpd_search_add_any_tag_constraint failed\n", __func__);
            goto search_done;
        } else if(mpd_search_add_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
                    3, title) == false) {
            printf("%s: mpd_search_add_any_tag_constraint failed\n", __func__);
            goto search_done;
        } else if(mpd_search_commit(conn) == false) {
            printf("%s: mpd_search_commit failed\n", __func__);
            goto search_done;
        } else {
            while((song = mpd_recv_song(conn)) != NULL) {
                mpd_song_free(song);
                if (cnt++ > 300) {
                    printf("%s: maximum count reached\n", __func__);
                    break;
                }
            }
        }
search_done:
        mpd_connection_free(conn);
        return cnt;
    }
    return -1;
}

int mpd_search_and_delete(char *artist, char * title)
{
    struct mpd_song *song;
    int cnt = 0;
    char uri[128] = "";
    struct mpd_connection *conn = setup_connection();
    if (conn) {
        if(mpd_search_db_songs(conn, false) == false) {
            goto search_done;
        } else if(mpd_search_add_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
                    0, artist) == false) {
            printf("%s: mpd_search_add_any_tag_constraint failed\n", __func__);
            goto search_done;
        } else if(mpd_search_add_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
                    3, title) == false) {
            printf("%s: mpd_search_add_any_tag_constraint failed\n", __func__);
            goto search_done;
        } else if(mpd_search_commit(conn) == false) {
            printf("%s: mpd_search_commit failed\n", __func__);
            goto search_done;
        } else {
            while((song = mpd_recv_song(conn)) != NULL) {
                sprintf(uri, "%s/%s", _MUSIC_PATH_, mpd_song_get_uri(song));
                printf("%s: try to remove %s", __func__, uri);
                if (remove(uri) != 0) {
                    printf(" - failed\n");
                } else {
                    printf(" - done\n");
                    cnt++;
                }
                mpd_song_free(song);
            }
        }
search_done:
        mpd_connection_free(conn);
    }
    return cnt;
}


