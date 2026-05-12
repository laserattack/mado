#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>

char *argv0;

#define STB_DS_IMPLEMENTATION
#include "thirdparty/stb_ds.h"

#include "thirdparty/arg.h"

#include "ast.h"

#define UNUSED(x) (void)(x)

// ================ TYPES

typedef struct Task {
    char *path; // absolute path to TASK.md
    char *name;
    uint16_t priority;
    char **tags; // dynarr
    char *status;
} Task;


// ================ GLOBAL VARS

static const char *main_dir_name = "TASKS";

static const char *task_dir_regex = "^[0-9]{8}T[0-9]{6}$";
static const char *name_regex = "^- NAME:[[:space:]]*(.*)$";
static const char *priority_regex = "^- PRIORITY:[[:space:]]*([0-9]{1,4})$";
static const char *tags_regex = "^- TAGS:[[:space:]]*(.*)$";
static const char *status_regex = "^- STATUS:[[:space:]]*(.*)$";

// ================ SOME USEFUL STUFF

static void die(const char *errstr, ...)
{
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

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

static void tasks_dir_init() {
    char *cwd = get_current_dir_name();
    char tasks_dir[PATH_MAX];
    snprintf(tasks_dir, sizeof(tasks_dir), "%s/%s", cwd, main_dir_name);

    if (file_exists(tasks_dir)) {
        printf("Tasks directory already exists: %s\n", tasks_dir);
    } else {
        if (mkdir(tasks_dir, 0755) == 0) {
            printf("Created tasks directory: %s\n", tasks_dir);
        } else {
            die("Failed to create tasks directory: %s", tasks_dir);
        }
    }

    free(cwd);
}

static void task_create_dir_and_md(const char *main_dir) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);

    char task_path[PATH_MAX-10];
    snprintf(task_path, sizeof(task_path), "%s/%s", main_dir, dir_name);

    if (file_exists(task_path)) die("Task directory already exists: %s", task_path);
    if (mkdir(task_path, 0755) != 0) die("Failed to create task directory: %s", task_path);

    char readme_path[PATH_MAX];
    snprintf(readme_path, sizeof(readme_path), "%s/TASK.md", task_path);

    FILE *f = fopen(readme_path, "w");
    if (!f) die("Failed to create TASK.md in: %s", task_path);

    fprintf(f, "- NAME:\n");
    fprintf(f, "- PRIORITY:\n");
    fprintf(f, "- TAGS:\n");
    fprintf(f, "- STATUS:\n");
    fclose(f);

    printf("%s:1:1\n", readme_path);
}

static Task *task_file_parse(const char *task_file) {
    FILE *f = fopen(task_file, "r");
    if (!f) return NULL;

    Task *task = calloc(1, sizeof(Task));
    task->priority = 0;
    task->path = strdup(task_file);

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

static Task **tasks_get_all(const char *main_dir) {
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

        Task *task = task_file_parse(task_file);
        if (task) {
            arrput(tasks, task);
        }
    }

    closedir(dir);
    return tasks;
}

static void task_free(Task *task) {
    if (!task) return;

    free(task->path);
    free(task->name);
    free(task->status);
    for (int j = 0; j < arrlen(task->tags); j++) {
        free(task->tags[j]);
    }
    arrfree(task->tags);
    free(task);
}

static void tasks_free(Task **tasks) {
    if (!tasks) return;

    for (int i = 0; i < arrlen(tasks); i++) {
        task_free(tasks[i]);
    }
    arrfree(tasks);
}

static int task_matches_condition(Task *task, ASTNode *node) {
    if (!node) return 1;

    switch (node->type) {
        case NODE_ALL:
            return 1;

        case NODE_BINARY_OP:
            switch (node->binary.op) {
                case OP_AND:
                    return task_matches_condition(task, node->binary.left) &&
                           task_matches_condition(task, node->binary.right);
                case OP_OR:
                    return task_matches_condition(task, node->binary.left) ||
                           task_matches_condition(task, node->binary.right);
                default: return 0;
            }

        case NODE_UNARY_OP:
            if (node->unary.op == OP_NOT) {
                return !task_matches_condition(task, node->unary.expr);
            }
            return 0;

        case NODE_COMPARISON:
            switch (node->comparison.field) {
                case CMP_PRIORITY: {
                    int task_val = task->priority;
                    int cond_val = node->comparison.value.int_value;
                    switch (node->comparison.cmp) {
                        case CMP_GT: return task_val > cond_val;
                        case CMP_LT: return task_val < cond_val;
                        case CMP_GE: return task_val >= cond_val;
                        case CMP_LE: return task_val <= cond_val;
                        case CMP_EQ: return task_val == cond_val;
                        default: return 0;
                    }
                }

                case CMP_TAG: {
                    char *cond_tag = node->comparison.value.str_value;
                    for (int i = 0; i < arrlen(task->tags); i++) {
                        if (strcmp(task->tags[i], cond_tag) == 0) {
                            return 1;
                        }
                    }
                    return 0;
                }

                case CMP_STATUS: {
                    char *cond_status = node->comparison.value.str_value;
                    if (!task->status) return 0;
                    return strcmp(task->status, cond_status) == 0;
                }

                default: return 0;
            }

        default: return 0;
    }
}

static Task **tasks_filter(Task **tasks, ASTNode *filter) {
    if (!tasks || arrlen(tasks) == 0 || !filter) {
        return tasks;
    }

    Task **filtered = NULL;
    for (int i = 0; i < arrlen(tasks); i++) {
        Task *t = tasks[i];
        if (task_matches_condition(t, filter)) {
            arrput(filtered, t);
        } else {
            task_free(t);
        }
    }

    arrfree(tasks);
    return filtered;
}

static void tasks_print(Task **tasks) {
    if (!tasks) return;

    for (int i = 0; i < arrlen(tasks); i++) {
        Task *t = tasks[i];
        printf("%s:1:1\n", t->path);
    }
}

// ================ ENTRYPOINT

static void usage(void) {
    die("usage: %s [-h] [-i] [-p query]\n"
        "  -h          show this help\n"
        "  -i          initialize TASKS directory in current location\n"
        "  -n          create new task\n"
        "  -p query    print tasks using query (e.g. 'priority > 5')",
        argv0);
}

int main(int argc, char **argv) {
    ARGBEGIN {
        case 'i': {
            tasks_dir_init();
            break;
        }
        case 'n': {
            // find main dir
            char *main_dir = find_dir_up(main_dir_name);
            if (!main_dir) die("Tasks directory not found");

            // create task
            task_create_dir_and_md(main_dir);

            // cleanup
            free(main_dir);
            break;
        }
        case 'p': {
            char *query = ARGF();

            // compile filter
            ASTNode *filter = parse(query);
            if (!filter) die("Failed to parse query: '%s'", query);

            // find main dir
            char *main_dir = find_dir_up(main_dir_name);
            if (!main_dir) die("Tasks directory not found");

            // find tasks
            Task **tasks = tasks_get_all(main_dir);
            tasks = tasks_filter(tasks, filter);
            tasks_print(tasks);

            // cleanup
            tasks_free(tasks);
            free(main_dir);
            ast_free(filter);
            break;
        }
        case 'h': {
            usage();
            break;
        }
        default: {
            die("Unknown flag '%c'", ARGC());
        }
    } ARGEND;

    return 0;
}
