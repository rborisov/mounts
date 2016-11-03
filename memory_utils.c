#include <stdlib.h>
#include <syslog.h>
#include "memory_utils.h"

size_t memory_write(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memstruct *mem = (struct MemoryStruct *)userp;

    syslog(LOG_DEBUG, "%i %i", size, nmemb);

    mem->memory = realloc(mem->memory, realsize + 1);
    if(mem->memory == NULL) {
        /* out of memory! */
        syslog(LOG_DEBUG, "not enough memory (realloc returned NULL)");
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

