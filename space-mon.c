#include <stdio.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "mpd-utils.h"
#include "db_utils.h"
#include "memory_utils.h"
#include "config.h"

pthread_t fs_listener_thread = 0;

struct memstruct rat_artist;
struct memstruct rat_title;

int free_disk_space()
{
    int id, cnt = 0;
    char *artist, *title;
/*    if (db_init() != 0) {
        printf("%s: SQLITE_ERROR\n", __func__);
        return 0;
    }*/
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
            goto done;
        }

        //next iteration
        id = db_get_song_by_rating_next();
    }
done:
//    db_close();
    return cnt;
}

void fs_stat_loop(void)
{
    struct statvfs sv;
    unsigned long fs_bavail;
    for (;;) {
        if (statvfs(_MUSIC_PATH_, &sv) == -1 ) {
            perror("stat");
        }
        fs_bavail = sv.f_bsize * sv.f_bavail;
        printf("%s: available space: %ld bytes\n", __func__, fs_bavail);
        if (fs_bavail < _FS_BYTES_AVAIL_) {
            printf("%s: lets clean fs!\n", __func__);
            free_disk_space();
        }
        sleep(5);
    }
}

int fs_listener_init(void)
{
    memory_init((void*)&rat_artist);
    memory_init((void*)&rat_title);
    if (pthread_create(&fs_listener_thread, NULL,
                (void*)fs_stat_loop, NULL) == 0) {
        pthread_detach(fs_listener_thread);
    } else {
        printf("%s: event_thread error\n", __func__);
        return -1;
    }
    if (db_init() != 0) {
        printf("%s: SQLITE_ERROR\n", __func__);
        return -1;
    }
    return 0;
}

void fs_listener_close(void)
{
    memory_clean((void*)&rat_artist);
    memory_clean((void*)&rat_title);
    db_close();
}
