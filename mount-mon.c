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
#include <mpd/client.h>
#include <assert.h>

#include "config.h"

#define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#define _(a) dgettext("org.gnunet.libextractor", a)

static struct EXTRACTOR_PluginList *plugins = NULL;
static EXTRACTOR_MetaDataProcessor processor = NULL;
static int in_process;

char artist[_MAXSTRING_] = "";
char title[_MAXSTRING_] = "";
char album[_MAXSTRING_] = "";

static struct mpd_connection * setup_connection(void)
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
    char searchstr[_MAXSTRING_] = "";
    int cnt = 0;

    struct mpd_connection *conn = setup_connection();

    if (conn) {
        sprintf(searchstr, "artist %s title %s", artist, title);
        if(mpd_search_db_songs(conn, false) == false) {
            printf("%s: mpd_search_db_songs failed\n", __func__);
            goto search_done;
        } else if(mpd_search_add_any_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
                    searchstr) == false) {
            printf("%s: mpd_search_add_any_tag_constraint failed\n", __func__);
            goto search_done;
        } else if(mpd_search_commit(conn) == false) {
            printf("%s: mpd_search_commit failed\n", __func__);
            goto search_done;
        } else {
            while((song = mpd_recv_song(conn)) != NULL) {
                printf("%s: mpd db found: %s\n", __func__, mpd_song_get_uri(song));
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

void send_to_server(const char *message)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("ERROR opening socket\n");
        goto done;
    }
    server = gethostbyname(_HOST_);
    if (server == NULL) {
        printf("ERROR, no such host\n");
        goto done;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(_PORT_);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        printf("ERROR connecting\n");
        goto done;
    }
    n = write(sockfd, message, strlen(message));
    if (n < 0)
        printf("ERROR writing to socket\n");
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

static int print_selected_keywords (void *cls,
        const char *plugin_name,
        enum EXTRACTOR_MetaType type,
        enum EXTRACTOR_MetaFormat format,
        const char *data_mime_type,
        const char *data,
        size_t data_len)
{
    char *keyword;
    const char *stype;
    const char *mt;

    mt = EXTRACTOR_metatype_to_string (type);
    stype = (NULL == mt) ? _("unknown") : gettext(mt);
    switch (format) {
        case EXTRACTOR_METAFORMAT_UTF8:
            keyword = strdup (data);
            if (NULL != keyword) {
                if (strcmp(stype, "artist") == 0)
                    strcpy(artist, keyword);
                if (strcmp(stype, "title") == 0)
                    strcpy(title, keyword);
                if (strcmp(stype, "album") == 0)
                    strcpy(album, keyword);
                free (keyword);
            }
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
        perror("setmntent");
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
        printf("0%3o\t%s\n", status->st_mode&0777, name);
        printf("%s: found %s\n", __func__, basename((char*)name));
        
        EXTRACTOR_extract (plugins,
                name,
                NULL, 0,
                processor,
                NULL);

        if (strcmp(artist, "") != 0 && strcmp(title, "") != 0) {
            if (isin_mpd_db(artist, title)) {
                //exists already; do nothing
                //TODO: check bitrate and duration
                return 0;
            }
            sprintf(path, "%s/%s", _MUSIC_PATH_, artist);
            mkdir(path, 0777);
            if (strcmp(album, "") != 0) {
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
                printf("%s: copied to %s\n", __func__, filename);
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
            printf("%s: there is no artist tag or title tag\n", __func__);
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
            fprintf(stdout, "Mount points changed. Count = %d\n", cnt1);
            if (cnt1 > cnt) {
                //we have new mount point
                printf("%d %d %s\n", cnt, cnt1, mentdir);
                ftw(mentdir, list, 1);
            }
            cnt = cnt1;
        }
        pfd.revents = 0;
    }

    close(mfd);
    EXTRACTOR_plugin_remove_all (plugins);

    return 0;
}
