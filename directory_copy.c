#include "mycp.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *join_paths(const char *parent, const char *name)
{
    size_t parent_len = strlen(parent);
    size_t name_len = strlen(name);
    int need_slash = parent_len > 0 && parent[parent_len - 1] != '/';
    size_t total = parent_len + (size_t)need_slash + name_len + 1;

    char *path = malloc(total);
    if (path == NULL) {
        perror("malloc");
        return NULL;
    }

    snprintf(path, total, "%s%s%s",
             parent, need_slash ? "/" : "", name);
    return path;
}

int copy_directory(const char *src, const char *dest)
{
    struct stat dest_stat;

    if (lstat(dest, &dest_stat) == -1) {
        if (errno != ENOENT) {
            perror(dest);
            return -1;
        }

        if (mkdir(dest, 0700) == -1) {
            perror(dest);
            return -1;
        }
    } else if (!S_ISDIR(dest_stat.st_mode)) {
        fprintf(stderr, "Destination is not a directory: %s\n", dest);
        return -1;
    }

    DIR *dir = opendir(src);
    if (dir == NULL) {
        perror(src);
        return -1;
    }

    int result = 0;
    struct dirent *entry;

    while (1) {
        errno = 0;
        entry = readdir(dir);

        if (entry == NULL) {
            if (errno != 0) {
                perror("readdir");
                result = -1;
            }
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *src_path = join_paths(src, entry->d_name);
        char *dest_path = join_paths(dest, entry->d_name);

        if (src_path == NULL || dest_path == NULL) {
            free(src_path);
            free(dest_path);
            result = -1;
            break;
        }

        if (copy_path(src_path, dest_path) == -1) {
            result = -1;
        }

        free(src_path);
        free(dest_path);

        if (result == -1) {
            break;
        }
    }

    if (closedir(dir) == -1) {
        perror("closedir");
        result = -1;
    }

    return result;
}
