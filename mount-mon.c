#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <mntent.h>
#include <string.h>
#include <ftw.h>
#include <libintl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <assert.h>
#include <ctype.h>
#include <syslog.h>
#include <id3v2lib.h>
#include <iconv.h>
#include <signal.h>
#include <execinfo.h>

#include "config.h"
#include "mpd-utils.h"
#include "space-mon.h"
#include "memory_utils.h"
#include "id3v1.h"

char artist[_MAXSTRING_];
char title[_MAXSTRING_];
char album[_MAXSTRING_];

void sigsegv_handler(int sig) {
    void *array[10];
    size_t size;
    char **funcs;
    int i;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    syslog(LOG_INFO, "Error: Received SIGSEGV signal %d:", sig);
//    backtrace_symbols_fd(array, size, STDOUT_FILENO);
    funcs = backtrace_symbols(array, size);
    for (i = 0; i < size; i++) {
        syslog(LOG_INFO, "%s", funcs[i]);
    }
    exit(1);
}

int file_copy(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0) {
        syslog(LOG_ERR, "%s: fd_from < 0", __func__);
        return -1;
    }
    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0) {
        syslog(LOG_ERR, "%s: fd_to < 0", __func__);
        goto out_error;
    }
    while (nread = read(fd_from, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;
        do {
            nwritten = write(fd_to, out_ptr, nread);
            if (nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            } else 
                if (errno != EINTR) {
                    syslog(LOG_ERR, "%s: write", __func__);
                    goto out_error;
                }
        } while (nread > 0);
    }
    if (nread == 0) {
        if (close(fd_to) < 0) {
            fd_to = -1;
            syslog(LOG_ERR, "%s: read", __func__);
            goto out_error;
        }
        close(fd_from);
        /* Success! */
        return 0;
    }
out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    syslog(LOG_ERR, "%s: errno %d", __func__, saved_errno);
    return -1;
}

char *trimwhitespace(char *str)
{
    char *end;
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0)  // All spaces?
        return str;
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    // Write new null terminator
    *(end+1) = 0;
    return str;
}

int get_mount_entry(char *ment)
{
    struct mntent *ent;
    FILE *aFile;
    int cnt = 0;

    aFile = setmntent(_MOUNTS_, "r");
    if (aFile == NULL) {
        syslog(LOG_INFO, "%s: setmntent", __func__);
        return -1;
    }
    while (NULL != (ent = getmntent(aFile))) {
        strcpy(ment, ent->mnt_dir);
        cnt++;
    }
    endmntent(aFile);

    return cnt;
}

const char *get_filename_ext(const char *filename) 
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void utf16tochar(uint16_t* string, int size, char *buf)
{
    char *_src = (char*)string;
    size_t dstlen = _MAXSTRING_;
    size_t srclen = size;
    char *_dst = buf;

    memset(buf, 0, _MAXSTRING_);
    iconv_t conv = iconv_open("", "");
    iconv(conv, &_src, &srclen, &_dst, &dstlen);
    if (strlen(buf) == 0) {
        iconv_close(conv);
        conv = iconv_open("UTF-8", "UTF-16");
        _src = string;
        srclen = size;
        _dst = buf;
        dstlen = _MAXSTRING_;
        iconv(conv, &_src, &srclen, &_dst, &dstlen);
    }
    buf[size] = '\0';
    printf("%d %d %s\n", dstlen, strlen(buf), buf);
    iconv_close(conv);
}


int list(const char *name, const struct stat *status, int type) 
{
    ID3v2_tag* tag2 = NULL;
    ID3v2_frame* artframe = NULL, *titframe = NULL, *albframe = NULL;
    ID3v2_frame_text_content* artcontent = NULL, 
        *titcontent = NULL, *albcontent = NULL;
    struct stat sb;
    char filename[2 * _MAXSTRING_] = "";
    char path[2 * _MAXSTRING_] = "";
    char mpdname[2 * _MAXSTRING_] = "";
    char *art = artist, *tit = title, *alb = album;
    if(type == FTW_F && strcmp(_EXT_, get_filename_ext(name)) == 0) {
        memset(artist, 0, sizeof(artist));
        memset(title, 0, sizeof(title));
        memset(album, 0, sizeof(album));

        //parse id3v2 tags
        tag2 = load_tag(name);
        if (tag2) {
            artframe = tag_get_artist(tag2);
            if (artframe) {
                artcontent = parse_text_frame_content(artframe);
                if (artcontent) {
                    utf16tochar(artcontent->data, artcontent->size, artist);
                    art = trimwhitespace(artist);
                    free(artcontent);
                }
                free(artframe);
            }
            titframe = tag_get_title(tag2);
            if (titframe) {
                titcontent = parse_text_frame_content(titframe);
                if (titcontent) {
                    utf16tochar(titcontent->data, titcontent->size, title);
                    tit = trimwhitespace(title);
                    free(titcontent);
                }
                free(titframe);
            }
            albframe = tag_get_album(tag2);
            if (albframe) {
                albcontent = parse_text_frame_content(albframe);
                if (albcontent) {
                    utf16tochar(albcontent->data, albcontent->size, album);
                    alb = trimwhitespace(album);
                    free(albcontent);
                }
                free(albframe);
            }
            free(tag2);
        }

        //parse id3v1 tags
        if (art[0] == 0 || tit[0] == 0) {
            struct id3v1_1 file_tag;
            if (id3v1parse(name, &file_tag) == EXIT_SUCCESS) {
                if (tit[0] == 0)
                    strcpy(tit, trimwhitespace(file_tag.title));
                if (art[0] == 0)
                    strcpy(art, trimwhitespace(file_tag.artist));
                if (alb[0] == 0)
                    strcpy(alb, trimwhitespace(file_tag.album));
                syslog(LOG_DEBUG, "%s: %s / %s / %s", __func__, art, alb, tit);
            }
        }

        //ca-st-ro requires artist and title; album is optional
        if (art[0] != 0 && tit[0] != 0) {
            if (isin_mpd_db(art, tit)) {
                //exists already; do nothing
                //TODO: check bitrate and duration
                return 0;
            }
            sprintf(path, "%s/%s", _MUSIC_PATH_, art);
            mkdir(path, 0777);
            if (alb[0] != 0) {
                sprintf(path, "%s/%s/%s", _MUSIC_PATH_, art, alb);
                mkdir(path, 0777);
                sprintf(mpdname, "%s/%s/%s.%s", art, alb, tit, _EXT_);
            } else {
                sprintf(mpdname, "%s/%s.%s", art, tit, _EXT_);
            }
            sprintf(filename, "%s/%s.%s", path, tit, _EXT_);
            if (stat(filename, &sb) == 0) {
                /* file already exists
                 * replace it if size less than size of new one 
                 * TBD: lets check for bitrate and duration instead of*/
                if (status->st_size <= sb.st_size) {
                    //new file is smaller
                    return 0;
                }
            }
            if (0 == file_copy(filename, name))
                syslog(LOG_INFO, "%s: new %s\n", __func__, filename);
            else {
                syslog(LOG_ERR, "%s: file copy error! %s\n", __func__, filename);
                return 0;
            }
            //add new song to mpd db
            if (0 != update_mpd_db(mpdname))
                return 0;
            //send new song info to socket
            send_to_server("[newfile]", mpdname);
        } else {
            syslog(LOG_INFO, "%s: %s there is no artist or title tag: %s/%s/%s\n", 
                    __func__, name, art, tit, alb);
        }
    }

    return 0;
}

int main()
{
    int mfd = open(_MOUNTS_, O_RDONLY, 0);
    struct pollfd pfd;
    int rv;
    char mentdir[128];

    signal(SIGSEGV, sigsegv_handler);

    fs_listener_init();

    int cnt1, cnt = get_mount_entry(mentdir);
    syslog(LOG_DEBUG, "%s: %d\n", __func__, cnt);

    pfd.fd = mfd;
    pfd.events = POLLERR | POLLPRI;
    pfd.revents = 0;
    while ((rv = poll(&pfd, 1, 5)) >= 0) {
        if (pfd.revents & POLLERR) {
            cnt1 = get_mount_entry(mentdir);
            if (cnt1 > cnt) {
                //we have new mount point
                syslog(LOG_DEBUG, "%s: %d %d %s\n", __func__, cnt, cnt1, mentdir);
                send_to_server("[mntpoint]", mentdir);
                ftw(mentdir, list, 1);
            } else {
                send_to_server("[mntpoint]", "closed");
            }
            cnt = cnt1;
        }
        pfd.revents = 0;
    }

    syslog(LOG_DEBUG, "%s: %d\n", __func__, rv);

    close(mfd);

    fs_listener_close();

    return 0;
}
