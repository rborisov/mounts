#include <stdio.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <unistd.h>
#include "mpd-utils.h"
#include "db_utils.h"
#include "memory_utils.h"
#include "config.h"

struct memstruct rat_artist;
struct memstruct rat_title;

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
        if (statvfs(_MUSIC_PATH_, &sv) == -1 ) {
            perror("stat");
        }
        fs_bavail = sv.f_bsize * sv.f_bavail;
        printf("%s: free space: %ld bytes\n", __func__, fs_bavail);
        if (fs_bavail < _FS_BYTES_AVAIL_) {
            printf("%s: lets clean fs!\n", __func__);
            free_disk_space();
        }
        sleep(5);
    }
}
/*
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
*/
