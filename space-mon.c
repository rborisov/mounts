#include <pthread.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <unistd.h>


int mpd_search_and_delete(char *artist, char * title)
{
    struct mpd_song *song;
    int cnt = 0;
    char uri[128] = "";
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
                sprintf(uri, "%s/%s", MUSICPATH, mpd_song_get_uri(song));
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
    }
search_done:
    return cnt;
}

int free_disk_space()
{
    int id, cnt = 0;
    char *artist, *title;
    id = db_get_prior_song_by_rating_first();
    while (id) {
        artist = db_get_song_artist(id);
        memory_write(artist, 1, strlen(artist), (void *)&rat_artist);
        title = db_get_song_name(id);
        memory_write(title, 1, strlen(title), (void *)&rat_title);
        //search in mpd
        //delete if exists
        cnt = mpd_search_and_delete(rat_artist.memory, rat_title.memory);
        if (cnt > 0) {
            printf("deleted %d songs\n", cnt);
            return cnt;
        }

        //next iteration
        id = db_get_song_by_rating_next();
    }
    return 0;
}

void fs_stat_loop(void)
{
    struct statvfs sv;
    unsigned long fs_bavail;
    int id;
    for (;;) {
        if (statvfs(MUSICPATH, &sv) == -1 ) {
            perror("stat");
        }
        fs_bavail = sv.f_bsize * sv.f_bavail;
        printf("%s: free space: %ld bytes\n", __func__, fs_bavail);
        if (fs_bavail < FSBYTESAVAIL) {
            printf("%s: lets clean fs!\n", __func__);
            free_disk_space();
        }
        sleep(5);
    }
}

int fs_listener_init(void)
{
    if (pthread_create(&fs_listener_thread, NULL,
                (void*)fs_stat_loop, NULL) == 0) {
        pthread_detach(fs_listener_thread);
    } else {
        printf("%s: event_thread error\n", __func__);
        goto ERROR;
    }
    return 0;
ERROR:
    return -1;
}

