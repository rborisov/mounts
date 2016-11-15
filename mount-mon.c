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

#include "config.h"
#include "mpd-utils.h"
#include "space-mon.h"
#include "memory_utils.h"

char artist[_MAXSTRING_];
char title[_MAXSTRING_];
char album[_MAXSTRING_];

int file_copy(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;
    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;
    while (nread = read(fd_from, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;
        do {
            nwritten = write(fd_to, out_ptr, nread);
            if (nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            } else 
                if (errno != EINTR)
                    goto out_error;
        } while (nread > 0);
    }
    if (nread == 0) {
        if (close(fd_to) < 0) {
            fd_to = -1;
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
    ID3v2_tag* tag = NULL;
    ID3v2_frame* frame;
    ID3v2_frame_text_content* content;
    struct stat sb;
    char filename[2 * _MAXSTRING_] = "";
    char path[2 * _MAXSTRING_] = "";
    char mpdname[2 * _MAXSTRING_] = "";
    if(type == FTW_F && strcmp(_EXT_, get_filename_ext(name)) == 0) {
        memset(artist, 0, sizeof(artist));
        memset(title, 0, sizeof(title));
        memset(album, 0, sizeof(album));

        tag = load_tag(name);
        if (tag == NULL)
            tag = new_tag();
        frame = tag_get_artist(tag);
        content = parse_text_frame_content(frame);
        if (content) {
            utf16tochar(content->data, content->size, artist);
            free(frame);
            free(content);
        }
        frame = tag_get_title(tag);
        content = parse_text_frame_content(frame);
        if (content) {
            utf16tochar(content->data, content->size, title);
            free(frame);
            free(content);
        }
        frame = tag_get_album(tag);
        content = parse_text_frame_content(frame);
        if (content) {
            utf16tochar(content->data, content->size, album);
            free(frame);
            free(content);
        }
        free(tag);

        if (artist[0] != 0 && title[0] != 0) {
            if (isin_mpd_db(artist, title)) {
                //exists already; do nothing
                //TODO: check bitrate and duration
                return 0;
            }
            sprintf(path, "%s/%s", _MUSIC_PATH_, artist);
            mkdir(path, 0777);
            if (album[0] != 0) {
                sprintf(path, "%s/%s/%s", _MUSIC_PATH_, artist, album);
                mkdir(path, 0777);
                sprintf(mpdname, "%s/%s/%s.%s", artist, album, title, _EXT_);
            } else {
                sprintf(mpdname, "%s/%s.%s", artist, title, _EXT_);
            }
            sprintf(filename, "%s/%s.%s", path, title, _EXT_);
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
                    __func__, name, artist, title, album);
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
