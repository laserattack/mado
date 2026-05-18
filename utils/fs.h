#ifndef FS_H
#define FS_H

void rmrf(const char *path);
int file_exists(const char *path);
char *find_dir_up(const char *dir_name);
char *get_cwd();

#ifdef FS_IMPL

#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
                     struct FTW *ftwbuf) {
    UNUSED(sb);
    UNUSED(typeflag);
    UNUSED(ftwbuf);
    return remove(fpath);
}

void rmrf(const char *path) { nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS); }

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

char *get_cwd(void) {
    char *dir = get_current_dir_name();
    if (!dir) {
        fprintf(stderr, "Failed to get current directory\n");
        exit(1);
    }
    return dir;
}

char *find_dir_up(const char *dir_name) {
    char *path = get_cwd();
    char *dir = strdup(path);
    char *result = NULL;

    while (1) {
        char test[PATH_MAX];
        snprintf(test, sizeof(test), "%s/%s", path, dir_name);

        if (file_exists(test)) {
            result = realpath(test, NULL);
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
