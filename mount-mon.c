#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <mntent.h>
#include <string.h>
#include <ftw.h>
#include <extractor.h>
#include <libintl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <assert.h>
#include <ctype.h>

#include "config.h"
#include "mpd-utils.h"
#include "space-mon.h"

#define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#define _(a) dgettext("org.gnunet.libextractor", a)

static struct EXTRACTOR_PluginList *plugins = NULL;
static EXTRACTOR_MetaDataProcessor processor = NULL;
static int in_process;

char artist[_MAXSTRING_];
char title[_MAXSTRING_];
char album[_MAXSTRING_];

void send_to_server(const char *message)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("%s: ERROR opening socket\n", __func__);
        goto done;
    }
    server = gethostbyname(_HOST_);
    if (server == NULL) {
        printf("%s: ERROR, no such host\n", __func__);
        goto done;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(_PORT_);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
//        printf("%s: ERROR connecting\n", __func__);
        goto done;
    }
    n = write(sockfd, message, strlen(message));
    if (n < 0)
        printf("%s: ERROR writing to socket\n", __func__);
done:
    close(sockfd);
    return;
}

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

static int print_selected_keywords (void *cls,
        const char *plugin_name,
        enum EXTRACTOR_MetaType type,
        enum EXTRACTOR_MetaFormat format,
        const char *data_mime_type,
        const char *data,
        size_t data_len)
{
    char *keyword, *keyw;
    const char *stype;
    const char *mt;

    mt = EXTRACTOR_metatype_to_string (type);
    stype = (NULL == mt) ? _("unknown") : gettext(mt);
    switch (format) {
        case EXTRACTOR_METAFORMAT_UTF8:
            keyw = strdup(data);
            keyword = trimwhitespace(keyw);
            if (NULL != keyword) {
                if (strcmp(stype, "artist") == 0)
                    strcpy(artist, keyword);
                if (strcmp(stype, "title") == 0)
                    strcpy(title, keyword);
                if (strcmp(stype, "album") == 0)
                    strcpy(album, keyword);
            }
            free (keyw);
            break;
        default:
            break;
    }

    return 0;
}

int get_mount_entry(char *ment)
{
    struct mntent *ent;
    FILE *aFile;
    int cnt = 0;

    aFile = setmntent(_MOUNTS_, "r");
    if (aFile == NULL) {
        printf("%s: setmntent", __func__);
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

int list(const char *name, const struct stat *status, int type) 
{
    struct stat sb;
    char filename[2 * _MAXSTRING_] = "";
    char path[2 * _MAXSTRING_] = "";
    char mpdname[2 * _MAXSTRING_] = "";
    if(type == FTW_F && strcmp(_EXT_, get_filename_ext(name)) == 0) {
//        printf("0%3o\t%s\n", status->st_mode&0777, name);
        //printf("%s: found %s\n", __func__, basename((char*)name));
       
        memset(artist, 0, sizeof(artist));
        memset(title, 0, sizeof(title));
        memset(album, 0, sizeof(album));

        EXTRACTOR_extract (plugins,
                name,
                NULL, 0,
                processor,
                NULL);

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
                printf("%s: new %s\n", __func__, filename);
            else {
                printf("%s: file copy error! %s\n", __func__, filename);
                return 0;
            }
            //add new song to mpd db
            if (0 != update_mpd_db(mpdname))
                return 0;
            //send new song info to socket
            send_to_server(filename);
        } else {
            printf("%s: %s there is no artist or title tag: %s/%s/%s\n", 
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

    plugins = EXTRACTOR_plugin_add_defaults (
            in_process ? EXTRACTOR_OPTION_IN_PROCESS : EXTRACTOR_OPTION_DEFAULT_POLICY);
    processor = &print_selected_keywords;

    int cnt1, cnt = get_mount_entry(mentdir);
    fprintf(stdout, "%d\n", cnt);

    pfd.fd = mfd;
    pfd.events = POLLERR | POLLPRI;
    pfd.revents = 0;
    while ((rv = poll(&pfd, 1, 5)) >= 0) {
        if (pfd.revents & POLLERR) {
            cnt1 = get_mount_entry(mentdir);
//            fprintf(stdout, "Mount points changed. Count = %d\n", cnt1);
            if (cnt1 > cnt) {
                //we have new mount point
                printf("%d %d %s\n", cnt, cnt1, mentdir);
                ftw(mentdir, list, 1);
            }
            cnt = cnt1;
        }
        pfd.revents = 0;
    }

    printf("%d\n", rv);

    close(mfd);
    EXTRACTOR_plugin_remove_all (plugins);

    fs_listener_close();

    return 0;
}
