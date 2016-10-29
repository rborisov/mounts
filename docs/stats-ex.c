#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>

int main(int argc, char *argv[])
{
    struct stat sb;
    struct statvfs sv;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pathname>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (stat(argv[1], &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }


    if (S_ISDIR(sb.st_mode)) {
        printf("directory\n");
        if (statvfs(argv[1], &sv) == -1 ) {
            perror("stat");
            exit(EXIT_FAILURE);
        }
        printf("free space: %ld bytes\n", (unsigned long)(sv.f_bsize * sv.f_bavail));
    } else
        printf("size: %lld bytes\n", (long long)sb.st_size);

    return 0;
}
