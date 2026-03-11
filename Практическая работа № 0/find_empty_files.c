#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void find_empty_files_recursive(void) {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat st;
        if (stat(entry->d_name, &st) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (chdir(entry->d_name) == -1) {
                perror("chdir");
                continue;
            }

            find_empty_files_recursive();

            if (chdir("..") == -1) {
                perror("chdir");
                closedir(dir);
                return;
            }
        } else if (S_ISREG(st.st_mode) && st.st_size == 0) {
            int fd = open(entry->d_name, O_RDONLY);
            if (fd == -1) {
                perror("open");
                continue;
            }
            close(fd);

            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                perror("getcwd");
                continue;
            }

            printf("%s/%s\n", cwd, entry->d_name);
        }
    }

    closedir(dir);
}

int main(void) {
    const char *home = getenv("HOME");
    if (home == NULL || *home == '\0') {
        fprintf(stderr, "Не удалось получить домашний каталог из HOME\n");
        return 1;
    }

    if (chdir(home) == -1) {
        perror("chdir");
        return 1;
    }

    find_empty_files_recursive();
    return 0;
}
