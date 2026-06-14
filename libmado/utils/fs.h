#ifndef FS_H
#define FS_H

int rmrf(const char *path);
int file_exists(const char *path);
char *find_dir_up(const char *dir_name);
char *get_cwd();

#ifdef FS_IMPL

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

int rmrf(const char *path) {
    struct stat st;
    DIR *dir;
    struct dirent *entry;
    char fullpath[PATH_MAX];

    if (lstat(path, &st) != 0)
        return -1;

    if (S_ISDIR(st.st_mode)) {
        dir = opendir(path);
        if (!dir)
            return -1;

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
            rmrf(fullpath);
        }
        closedir(dir);
        return rmdir(path);
    } else {
        return unlink(path);
    }
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

char *get_cwd(void) {
    char *buf = NULL;
    size_t size = 128;

    while (1) {
        buf = malloc(size);

        if (getcwd(buf, size) != NULL)
            return buf;

        free(buf);

        if (errno != ERANGE) {
            fprintf(stderr, "Failed to get current directory: %s\n",
                    strerror(errno));
            exit(1);
        }

        size *= 2;
    }
}

char *find_dir_up(const char *dir_name) {
    char *path = get_cwd();
    char *dir = strdup(path);
    char *result = NULL;

    while (1) {
        char test[PATH_MAX];
        snprintf(test, sizeof(test), "%s/%s", path, dir_name);

        if (file_exists(test)) {
            result = strdup(test);
            goto cleanup;
        }

        strcpy(dir, path);
        char *parent = dirname(dir);
        if (strcmp(path, parent) == 0)
            break;
        strcpy(path, parent);
    }

cleanup:
    free(path);
    free(dir);
    return result;
}

#endif // FS_IMPL

#endif // FS_H
