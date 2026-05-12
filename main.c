#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>

#define STB_DS_IMPLEMENTATION
#include "thirdparty/stb_ds.h"

#include "ast.h"


#define UNUSED(x) (void)(x)

// ================ TYPES

typedef struct Task {
    char *name;
    uint16_t priority;
    char **tags; // dynarr
    char *status;
} Task;


// ================ GLOBAL VARS

static const char *main_dir_name = "TAMD";

static const char *task_dir_regex = "^[0-9]{8}T[0-9]{6}$";
static const char *name_regex = "^- NAME:[[:space:]]*(.*)$";
static const char *priority_regex = "^- PRIORITY:[[:space:]]*([0-9]{1,4})$";
static const char *tags_regex = "^- TAGS:[[:space:]]*(.*)$";
static const char *status_regex = "^- STATUS:[[:space:]]*(.*)$";

// ================ SOME USEFUL STUFF

static char *trim(char *str) {
    if (!str) return str;

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}


// ================ REGEX

static int regex_match(const char *name, const char *regex_str) {
    regex_t regex;
    int ret;

    regcomp(&regex, regex_str, REG_EXTENDED);
    ret = regexec(&regex, name, 0, NULL, 0);
    regfree(&regex);

    return ret == 0;
}

static char *regex_extract_first_group(const char *line, const char *pattern) {
    regex_t regex;
    regmatch_t matches[2];
    int ret;
    char *result = NULL;

    regcomp(&regex, pattern, REG_EXTENDED);
    ret = regexec(&regex, line, 2, matches, 0);

    if (ret == 0 && matches[1].rm_so != -1) {
        int len = matches[1].rm_eo - matches[1].rm_so;
        result = malloc(len + 1);
        strncpy(result, line + matches[1].rm_so, len);
        result[len] = '\0';
    }

    regfree(&regex);
    return result;
}

// ================ WORK WITH FS

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// returns normalized absolute path like '/home/serr/projects/TAMD'
static char *find_dir_up(const char *dir_name) {
    char *path = get_current_dir_name(); // this is bullshit, returns full path, not just name
    char *dir = strdup(path);
    char *result = NULL;

    while (1) {
        char test[PATH_MAX];
        sprintf(test, "%s/%s", path, dir_name);

        if (file_exists(test)) {
            result = realpath(test, NULL);
            goto RET;
        }

        strcpy(dir, path);
        char *parent = dirname(dir);
        if (strcmp(path, parent) == 0) break;
        strcpy(path, parent);
    }

RET:
    free(path);
    free(dir);
    return result;
}

// ================ MAIN LOGIC

static Task *parse_task_file(const char *task_file) {
    FILE *f = fopen(task_file, "r");
    if (!f) return NULL;

    Task *task = calloc(1, sizeof(Task));
    task->priority = 0;

    char *line = NULL;
    size_t line_len = 0;
    ssize_t read;

    while ((read = getline(&line, &line_len, f)) != -1) {
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        char *value;

        if ((value = regex_extract_first_group(line, name_regex)) != NULL) {
            free(task->name);
            task->name = strdup(trim(value));
            free(value);
        }
        else if ((value = regex_extract_first_group(line, priority_regex)) != NULL) {
            task->priority = atoi(value);
            free(value);
        }
        else if ((value = regex_extract_first_group(line, tags_regex)) != NULL) {
            char *tags_str = trim(value);
            if (tags_str && *tags_str) {
                char *saveptr;
                char *token = strtok_r(tags_str, ",", &saveptr);
                while (token) {
                    char *clean = trim(token);
                    if (*clean) {
                        arrput(task->tags, strdup(clean));
                    }
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            free(value);
        }
        else if ((value = regex_extract_first_group(line, status_regex)) != NULL) {
            free(task->status);
            task->status = strdup(trim(value));
            free(value);
        }
    }

    free(line);
    fclose(f);

    return task;
}

static Task **get_all_tasks(const char *main_dir) {
    DIR *dir = opendir(main_dir);
    if (!dir) return NULL;

    Task **tasks = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            !regex_match(entry->d_name, task_dir_regex)) {
            continue;
        }

        char task_file[PATH_MAX];
        snprintf(task_file, sizeof(task_file), "%s/%s/TASK.md", main_dir, entry->d_name);

        if (!file_exists(task_file)) {
            continue;
        }

        Task *task = parse_task_file(task_file);
        if (task) {
            arrput(tasks, task);
        }
    }

    closedir(dir);
    return tasks;
}

static void free_tasks(Task **tasks) {
    if (!tasks) return;

    for (int i = 0; i < arrlen(tasks); i++) {
        Task *t = tasks[i];
        free(t->name);
        free(t->status);
        for (int j = 0; j < arrlen(t->tags); j++) {
            free(t->tags[j]);
        }
        arrfree(t->tags);
        free(t);
    }
    arrfree(tasks);
}

static void print_tasks(Task **tasks) {
    if (!tasks) {
        printf("No tasks found\n");
        return;
    }

    for (int i = 0; i < arrlen(tasks); i++) {
        Task *t = tasks[i];
        printf("NAME: %s\n", t->name ? t->name : "");
        printf("PRIORITY: %d\n", t->priority);
        printf("STATUS: %s\n", t->status ? t->status : "");
        printf("TAGS: ");
        if (arrlen(t->tags) == 0) {
            printf("");
        } else {
            for (int j = 0; j < arrlen(t->tags); j++) {
                printf("%s ", t->tags[j]);
            }
        }
        printf("\n\n");
    }
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    char *main_dir = find_dir_up(main_dir_name);
    if (!main_dir) {
        fprintf(stderr, "main directory not found\n");
        return 1;
    }

    Task **tasks = get_all_tasks(main_dir);
    print_tasks(tasks);

    free_tasks(tasks);
    free(main_dir);
    return 0;
}
