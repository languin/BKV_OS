#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int append_component(char *path, const char *name) {
    size_t base = strlen(path);
    size_t name_len = strlen(name);

    if (strcmp(path, "/") != 0) {
        if (base + 1 + name_len >= PATH_MAX) {
            return -1;
        }
        path[base++] = '/';
        path[base] = '\0';
    } else if (base + name_len >= PATH_MAX) {
        return -1;
    }

    memcpy(path + base, name, name_len + 1);
    return 0;
}

static int build_absolute_path_from_current_dir(char *path) {
    struct stat cur;
    struct stat parent;

    if (stat(".", &cur) == -1 || stat("..", &parent) == -1) {
        return -1;
    }

    if (cur.st_ino == parent.st_ino && cur.st_dev == parent.st_dev) {
        strcpy(path, "/");
        return 0;
    }

    if (chdir("..") == -1) {
        return -1;
    }

    DIR *dir = opendir(".");
    if (dir == NULL) {
        return -1;
    }

    char name[NAME_MAX + 1];
    int found = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat st;
        if (stat(entry->d_name, &st) == -1) {
            continue;
        }

        if (st.st_ino == cur.st_ino && st.st_dev == cur.st_dev) {
            strncpy(name, entry->d_name, NAME_MAX);
            name[NAME_MAX] = '\0';
            found = 1;
            break;
        }
    }

    closedir(dir);

    if (!found) {
        return -1;
    }

    if (build_absolute_path_from_current_dir(path) == -1) {
        return -1;
    }

    if (append_component(path, name) == -1) {
        return -1;
    }

    if (chdir(name) == -1) {
        return -1;
    }

    return 0;
}

static void find_empty_files_recursive(char *current_path) {
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
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            char saved_path[PATH_MAX];
            strncpy(saved_path, current_path, PATH_MAX - 1);
            saved_path[PATH_MAX - 1] = '\0';

            if (append_component(current_path, entry->d_name) == -1) {
                continue;
            }

            if (chdir(entry->d_name) == -1) {
                strncpy(current_path, saved_path, PATH_MAX - 1);
                current_path[PATH_MAX - 1] = '\0';
                continue;
            }

            find_empty_files_recursive(current_path);

            if (chdir("..") == -1) {
                closedir(dir);
                return;
            }

            strncpy(current_path, saved_path, PATH_MAX - 1);
            current_path[PATH_MAX - 1] = '\0';
        } else if (S_ISREG(st.st_mode) && st.st_size == 0) {
            int fd = open(entry->d_name, O_RDONLY);
            if (fd == -1) {
                continue;
            }
            close(fd);

            if (strcmp(current_path, "/") == 0) {
                printf("/%s\n", entry->d_name);
            } else {
                printf("%s/%s\n", current_path, entry->d_name);
            }
        }
    }

    closedir(dir);
}

int main(void) {
    char current_path[PATH_MAX];

    /* Программа запускается из домашнего каталога пользователя. */
    if (build_absolute_path_from_current_dir(current_path) == -1) {
        fprintf(stderr, "Не удалось определить начальный путь\n");
        return 1;
    }

    find_empty_files_recursive(current_path);
    return 0;
}
