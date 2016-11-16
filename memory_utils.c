#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "memory_utils.h"
#include "config.h"

size_t memory_write(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memstruct *mem = (struct MemoryStruct *)userp;

    syslog(LOG_DEBUG, "%s: %d %d", __func__, (int)size, (int)nmemb);

    mem->memory = realloc(mem->memory, realsize + 1);
    if(mem->memory == NULL) {
        /* out of memory! */
        syslog(LOG_ERR, "%s: not enough memory (realloc returned NULL)", __func__);
        return 0;
    }

    memcpy(mem->memory, contents, realsize);
    mem->size = realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void memory_init(void *userp)
{
    struct memstruct *mem = (struct MemoryStruct *)userp;
    mem->memory = malloc(1);
}

void memory_clean(void *userp)
{
    struct memstruct *mem = (struct MemoryStruct *)userp;
    free(mem->memory);
}

void send_to_server(const char *type, const char *message)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "%s: ERROR opening socket\n", __func__);
        goto done;
    }
    server = gethostbyname(_HOST_);
    if (server == NULL) {
        syslog(LOG_ERR, "%s: ERROR, no such host\n", __func__);
        goto done;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(_PORT_);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        syslog(LOG_ERR, "%s: ERROR connecting\n", __func__);
        goto done;
    }
    n = write(sockfd, type, strlen(type));
    if (n) {
        n = write(sockfd, message, strlen(message));
    }
    if (n < 0)
        syslog(LOG_ERR, "%s: ERROR writing to socket\n", __func__);
done:
    close(sockfd);
    return;
}

