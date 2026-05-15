#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdio.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>
#include <ftw.h>

char *argv0;

#define STB_DS_IMPLEMENTATION
#include "thirdparty/stb_ds.h"
#include "thirdparty/arg.h"

#include "ast.h"

#define UNUSED(x) (void)(x)

// ================ TYPES

typedef struct Task {
    char *path; // absolute path to task dir
    char *name;
    uint16_t priority;
    char **tags; // dynarr
    char *status;
} Task;


// ================ GLOBAL VARS

static const char *g_main_dir_name = "TASKS";
static const int g_max_header_lines = 30;

// precompiled regexp for file parsing
static regex_t g_task_dir_regex;
static regex_t g_name_regex;
static regex_t g_priority_regex;
static regex_t g_tags_regex;
static regex_t g_status_regex;

static int g_regex_initialized = 0;

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

static void init_regexes(void) {
    if (g_regex_initialized) return;

    if (regcomp(&g_task_dir_regex, "^[0-9]{8}T[0-9]{6}$", REG_EXTENDED) != 0)
        die("Failed to compile task_dir regex");
    if (regcomp(&g_name_regex, "^- NAME:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        die("Failed to compile name regex");
    if (regcomp(&g_priority_regex, "^- PRIORITY:[[:space:]]*([0-9]{1,3})$", REG_EXTENDED) != 0)
        die("Failed to compile priority regex");
    if (regcomp(&g_tags_regex, "^- TAGS:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        die("Failed to compile tags regex");
    if (regcomp(&g_status_regex, "^- STATUS:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        die("Failed to compile status regex");

    g_regex_initialized = 1;
}

static void free_regexes(void) {
    if (!g_regex_initialized) return;

    regfree(&g_task_dir_regex);
    regfree(&g_name_regex);
    regfree(&g_priority_regex);
    regfree(&g_tags_regex);
    regfree(&g_status_regex);
}

static int regex_match(const char *name, regex_t *regex) {
    return regexec(regex, name, 0, NULL, 0) == 0;
}

static char *regex_extract_first_group(const char *line, regex_t *regex) {
    regmatch_t matches[2];

    if (regexec(regex, line, 2, matches, 0) == 0 && matches[1].rm_so != -1) {
        int len = matches[1].rm_eo - matches[1].rm_so;
        char *result = malloc(len + 1);
        if (result) {
            strncpy(result, line + matches[1].rm_so, len);
            result[len] = '\0';
        }
        return result;
    }
    return NULL;
}

// ================ WORK WITH FS

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    UNUSED(sb);
    UNUSED(typeflag);
    UNUSED(ftwbuf);
    return remove(fpath);
}

static void rmrf(const char *path) {
    nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

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
        snprintf(test, sizeof(test), "%s/%s", path, dir_name);

        if (file_exists(test)) {
            result = realpath(test, NULL);
            goto cleanup;
        }

        strcpy(dir, path);
        char *parent = dirname(dir);
        if (strcmp(path, parent) == 0) break;
        strcpy(path, parent);
    }

cleanup:
    free(path);
    free(dir);
    return result;
}

// ================ MAIN LOGIC

static void tasks_dir_init() {
    char *cwd = get_current_dir_name();
    char tasks_dir[PATH_MAX];
    snprintf(tasks_dir, sizeof(tasks_dir), "%s/%s", cwd, g_main_dir_name);

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

    printf("%s:1:1: STATUS:[] NAME:[] PRIORITY:[0] TAGS:[]\n", readme_path);
}

static Task *task_parse(const char *task_dir) {
    char task_file[PATH_MAX];
    snprintf(task_file, sizeof(task_file), "%s/TASK.md", task_dir);

    FILE *f = fopen(task_file, "r");
    if (!f) return NULL;

    Task *task = calloc(1, sizeof(Task));
    task->priority = 0;
    task->path = strdup(task_dir);
    task->status = strdup("");
    task->name = strdup("");

    char *line = NULL;
    size_t line_len = 0;
    ssize_t read;

    int lines_processed = 0;

    while (lines_processed < g_max_header_lines && (read = getline(&line, &line_len, f)) != -1) {
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        lines_processed++;

        char *value;

        if ((value = regex_extract_first_group(line, &g_name_regex)) != NULL) {
            free(task->name);
            task->name = strdup(trim(value));
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_priority_regex)) != NULL) {
            task->priority = atoi(value);
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_tags_regex)) != NULL) {
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
        } else if ((value = regex_extract_first_group(line, &g_status_regex)) != NULL) {
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
            !regex_match(entry->d_name, &g_task_dir_regex)) {
            continue;
        }

        char task_dir[PATH_MAX-10];
        snprintf(task_dir, sizeof(task_dir), "%s/%s", main_dir, entry->d_name);

        char task_file[PATH_MAX];
        snprintf(task_file, sizeof(task_file), "%s/TASK.md", task_dir);

        if (!file_exists(task_file)) continue;

        Task *task = task_parse(task_dir);
        if (task) arrput(tasks, task);
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

// query language iterpreter here
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

        case NODE_LIST_COMPARISON: {
            ComparisonOperator cmp = node->list_comparison.cmp;

            if (node->list_comparison.field == CMP_PRIORITY) {
                NumList *list = node->list_comparison.num_list;

                if (cmp == CMP_EQ) {
                    for (int i = 0; i < list->count; i++) {
                        if (task->priority == list->items[i]) return 1;
                    }
                    return 0;
                } else if (cmp == CMP_NE) {
                    for (int i = 0; i < list->count; i++) {
                        if (task->priority == list->items[i]) return 0;
                    }
                    return 1;
                }
                return 0;
            } else {
                StringList *list = node->list_comparison.str_list;

                if (node->list_comparison.field == CMP_TAG) {

                    // Special case: empty string means "no tags"
                    int has_empty = 0;
                    for (int i = 0; i < list->count; i++) {
                        if (list->items[i] && list->items[i][0] == '\0') {
                            has_empty = 1;
                            break;
                        }
                    }
                    if (has_empty) {
                        int has_no_tags = arrlen(task->tags) == 0;
                        if (cmp == CMP_EQ) return has_no_tags;
                        if (cmp == CMP_NE) return !has_no_tags;
                        if (cmp == CMP_SUBSTR) return 1;
                        if (cmp == CMP_NSUBSTR) return 0;
                    }

                    if (cmp == CMP_EQ) {
                        for (int j = 0; j < arrlen(task->tags); j++) {
                            for (int i = 0; i < list->count; i++) {
                                if (strcmp(task->tags[j], list->items[i]) == 0) return 1;
                            }
                        }
                        return 0;
                    } else if (cmp == CMP_NE) {
                        for (int j = 0; j < arrlen(task->tags); j++) {
                            for (int i = 0; i < list->count; i++) {
                                if (strcmp(task->tags[j], list->items[i]) == 0) return 0;
                            }
                        }
                        return 1;
                    } else if (cmp == CMP_SUBSTR) {
                        for (int j = 0; j < arrlen(task->tags); j++) {
                            for (int i = 0; i < list->count; i++) {
                                if (strstr(task->tags[j], list->items[i]) != NULL) return 1;
                            }
                        }
                        return 0;
                    } else if (cmp == CMP_NSUBSTR) {
                        for (int j = 0; j < arrlen(task->tags); j++) {
                            for (int i = 0; i < list->count; i++) {
                                if (strstr(task->tags[j], list->items[i]) != NULL) return 0;
                            }
                        }
                        return 1;
                    }
                    return 0;
                } else {
                    char *task_value = (node->list_comparison.field == CMP_STATUS) ? task->status : task->name;
                    if (!task_value) return (cmp == CMP_NE || cmp == CMP_NSUBSTR) ? 1 : 0;

                    if (cmp == CMP_EQ) {
                        for (int i = 0; i < list->count; i++) {
                            if (strcmp(task_value, list->items[i]) == 0) return 1;
                        }
                        return 0;
                    } else if (cmp == CMP_NE) {
                        for (int i = 0; i < list->count; i++) {
                            if (strcmp(task_value, list->items[i]) == 0) return 0;
                        }
                        return 1;
                    } else if (cmp == CMP_SUBSTR) {
                        for (int i = 0; i < list->count; i++) {
                            if (strstr(task_value, list->items[i]) != NULL) return 1;
                        }
                        return 0;
                    } else if (cmp == CMP_NSUBSTR) {
                        for (int i = 0; i < list->count; i++) {
                            if (strstr(task_value, list->items[i]) != NULL) return 0;
                        }
                        return 1;
                    }
                    return 0;
                }
            }
        }

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
                        case CMP_NE: return task_val != cond_val;
                        default: return 0;
                    }
                }

                case CMP_TAG: {
                    char *cond_tag = node->comparison.value.str_value;

                    // Special case: empty string means "no tags"
                    if (cond_tag && cond_tag[0] == '\0') {
                        int has_no_tags = arrlen(task->tags) == 0;
                        if (node->comparison.cmp == CMP_EQ) return has_no_tags;
                        if (node->comparison.cmp == CMP_NE) return !has_no_tags;
                        if (node->comparison.cmp == CMP_SUBSTR) return 1;
                        if (node->comparison.cmp == CMP_NSUBSTR) return 0;
                    }

                    for (int i = 0; i < arrlen(task->tags); i++) {
                        if (node->comparison.cmp == CMP_EQ) {
                            if (strcmp(task->tags[i], cond_tag) == 0) return 1;
                        } else if (node->comparison.cmp == CMP_NE) {
                            if (strcmp(task->tags[i], cond_tag) == 0) return 0;
                        } else if (node->comparison.cmp == CMP_SUBSTR) {
                            if (strstr(task->tags[i], cond_tag) != NULL) return 1;
                        } else if (node->comparison.cmp == CMP_NSUBSTR) {
                            if (strstr(task->tags[i], cond_tag) != NULL) return 0;
                        }
                    }
                    return (node->comparison.cmp == CMP_NE || node->comparison.cmp == CMP_NSUBSTR);
                }

                case CMP_STATUS: {
                    char *cond_status = node->comparison.value.str_value;
                    if (!task->status) return 0;
                    if (node->comparison.cmp == CMP_EQ) {
                        return strcmp(task->status, cond_status) == 0;
                    } else if (node->comparison.cmp == CMP_NE) {
                        return strcmp(task->status, cond_status) != 0;
                    } else if (node->comparison.cmp == CMP_SUBSTR) {
                        return strstr(task->status, cond_status) != NULL;
                    } else if (node->comparison.cmp == CMP_NSUBSTR) {
                        return strstr(task->status, cond_status) == NULL;
                    }
                    return 0;
                }

                case CMP_NAME: {
                    char *cond_name = node->comparison.value.str_value;
                    if (!task->name) return 0;
                    if (node->comparison.cmp == CMP_EQ) {
                        return strcmp(task->name, cond_name) == 0;
                    } else if (node->comparison.cmp == CMP_NE) {
                        return strcmp(task->name, cond_name) != 0;
                    } else if (node->comparison.cmp == CMP_SUBSTR) {
                        return strstr(task->name, cond_name) != NULL;
                    } else if (node->comparison.cmp == CMP_NSUBSTR) {
                        return strstr(task->name, cond_name) == NULL;
                    }
                    return 0;
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

typedef void (*task_operation_fn)(Task **tasks, void *ctx);

static void task_op_print(Task **tasks, void *ctx) {
    UNUSED(ctx);
    if (!tasks) return;

    for (int i = 0; i < arrlen(tasks); i++) {
        Task *t = tasks[i];

        printf("%s/TASK.md:1:1: STATUS:[%s] NAME:[%s] PRIORITY:[%d] TAGS:[", t->path, t->status, t->name, t->priority);

        for (int j = 0; j < arrlen(t->tags); j++) {
            printf("%s", t->tags[j]);
            if (j < arrlen(t->tags) - 1) {
                printf(",");
            }
        }
        printf("]\n");
    }
}

static void task_op_delete(Task **tasks, void *ctx) {
    UNUSED(ctx);

    if (!tasks) return;

    for (int i = 0; i < arrlen(tasks); i++) {
        Task *t = tasks[i];
        rmrf(t->path);
        printf("Removed: %s\n", t->path);
    }
}

static void tasks_process_with_filter(const char *query, task_operation_fn op, void *ctx) {
    // compile filter
    ASTNode *filter = parse(query);
    if (!filter) die("Failed to parse query: '%s'", query);

    // find main dir
    char *main_dir = find_dir_up(g_main_dir_name);
    if (!main_dir) die("Tasks directory not found");

    // find tasks
    Task **tasks = tasks_get_all(main_dir);
    tasks = tasks_filter(tasks, filter);

    // execute operation
    op(tasks, ctx);

    // cleanup
    tasks_free(tasks);
    free(main_dir);
    ast_free(filter);
}

// ================ ENTRYPOINT

static void usage(void) {
    die("usage: %s [-h] [-i] [n] [-p query] [-r query]\n"
        "  -h          show this help\n"
        "  -i          initialize main directory in current location\n"
        "  -n          create new task\n"
        "  -p query    print tasks using query (e.g. 'priority > 5')\n"
        "  -r query    remove tasks matching query",
        argv0);
}

int main(int argc, char **argv) {
    init_regexes();

    ARGBEGIN {
        case 'h': {
            usage();
            goto cleanup;
        }
        case 'i': {
            tasks_dir_init();
            goto cleanup;
        }
        case 'n': {
            char *main_dir = find_dir_up(g_main_dir_name);
            if (!main_dir) die("Tasks directory not found");
            task_create_dir_and_md(main_dir);
            free(main_dir);
            goto cleanup;
        }
        case 'p': {
            char *query = ARGF();
            if (!query) die("-p requires a query argument");
            tasks_process_with_filter(query, task_op_print, NULL);
            goto cleanup;
        }
        case 'r': {
            char *query = ARGF();
            if (!query) die("-r requires a query argument");
            tasks_process_with_filter(query, task_op_delete, NULL);
            goto cleanup;
        }
        default: {
            die("Unknown flag '%c'", ARGC());
        }
    } ARGEND;

    usage();

cleanup:
    free_regexes();
    return 0;
}
