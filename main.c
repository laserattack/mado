#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

char *argv0;

#include "utils/arg.h"
#include "utils/da.h"

#define FS_IMPL
#include "utils/fs.h"

#define UTIL_IMPL
#include "utils/util.h"

#define REGEX_IMPL
#include "utils/regex.h"

#include "ast.h"

#define UNUSED(x) (void)(x)

// ================ TYPES

typedef enum {
    FMT_UNIX,
    FMT_ONLY_PATH,
} OutputFormat;

typedef struct Task {
    char *path; // absolute path to task dir
    char *name;
    uint16_t priority;
    char **tags; // dynarr
    char *status;
    char *time;
} Task;

// ================ GLOBAL

static const char *g_main_dir_name = "TASKS";
static const int g_max_header_lines = 30;

// precompiled regexp for file parsing
static regex_t g_task_dir_regex;
static regex_t g_name_regex;
static regex_t g_priority_regex;
static regex_t g_tags_regex;
static regex_t g_status_regex;

static int g_regex_initialized = 0;

static void init_regexes() {
    if (g_regex_initialized)
        return;

    if (regcomp(&g_task_dir_regex, "^[0-9]{8}T[0-9]{6}$", REG_EXTENDED) != 0)
        die("Failed to compile task_dir regex");
    if (regcomp(&g_name_regex, "^- NAME:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        die("Failed to compile name regex");
    if (regcomp(&g_priority_regex, "^- PRIORITY:[[:space:]]*([0-9]{1,3})$",
                REG_EXTENDED) != 0)
        die("Failed to compile priority regex");
    if (regcomp(&g_tags_regex, "^- TAGS:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        die("Failed to compile tags regex");
    if (regcomp(&g_status_regex, "^- STATUS:[[:space:]]*(.*)$", REG_EXTENDED) !=
        0)
        die("Failed to compile status regex");

    g_regex_initialized = 1;
}

static void free_regexes() {
    if (!g_regex_initialized)
        return;

    regfree(&g_task_dir_regex);
    regfree(&g_name_regex);
    regfree(&g_priority_regex);
    regfree(&g_tags_regex);
    regfree(&g_status_regex);
}

// ================ INTERPRETER

static void tasks_dir_init() {
    char *cwd = get_current_dir_name();
    char tasks_dir[PATH_MAX];
    snprintf(tasks_dir, sizeof(tasks_dir), "%s/%s", cwd, g_main_dir_name);

    if (file_exists(tasks_dir)) {
        printf("Tasks directory already exists: %s\n", tasks_dir);
    } else {
        if (mkdir(tasks_dir, 0755) == 0)
            printf("Created tasks directory: %s\n", tasks_dir);
        else
            die("Failed to create tasks directory: %s", tasks_dir);
    }

    free(cwd);
}

static void task_create_dir_and_md(const char *main_dir) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);

    char task_path[PATH_MAX - 10];
    snprintf(task_path, sizeof(task_path), "%s/%s", main_dir, dir_name);

    if (file_exists(task_path))
        die("Task directory already exists: %s", task_path);
    if (mkdir(task_path, 0755) != 0)
        die("Failed to create task directory: %s", task_path);

    char readme_path[PATH_MAX];
    snprintf(readme_path, sizeof(readme_path), "%s/TASK.md", task_path);

    FILE *f = fopen(readme_path, "w");
    if (!f)
        die("Failed to create TASK.md in: %s", task_path);

    fprintf(f, "- NAME:\n");
    fprintf(f, "- PRIORITY:\n");
    fprintf(f, "- TAGS:\n");
    fprintf(f, "- STATUS:\n");
    fclose(f);

    printf("%s:1:1: STATUS:[] NAME:[] PRIORITY:[0] TAGS:[]\n", readme_path);
}

static Task *task_parse(const char *task_dir, const char *task_time) {
    char task_file[PATH_MAX];
    snprintf(task_file, sizeof(task_file), "%s/TASK.md", task_dir);

    FILE *f = fopen(task_file, "r");
    if (!f)
        return NULL;

    Task *task = calloc(1, sizeof(Task));
    task->priority = 0;
    task->path = strdup(task_dir);
    task->status = strdup("");
    task->name = strdup("");
    task->time = strdup(task_time);

    char *line = NULL;
    size_t line_len = 0;
    ssize_t read;

    int lines_processed = 0;

    while (lines_processed < g_max_header_lines &&
           (read = getline(&line, &line_len, f)) != -1) {
        if (read > 0 && line[read - 1] == '\n')
            line[read - 1] = '\0';
        lines_processed++;

        char *value;

        if ((value = regex_extract_first_group(line, &g_name_regex)) != NULL) {
            free(task->name);
            task->name = strdup(trim(value));
            free(value);
        } else if ((value = regex_extract_first_group(
                        line, &g_priority_regex)) != NULL) {
            task->priority = atoi(value);
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_tags_regex)) !=
                   NULL) {
            char *tags_str = trim(value);
            if (tags_str && *tags_str) {
                char *saveptr;
                char *token = strtok_r(tags_str, ",", &saveptr);
                while (token) {
                    char *clean = trim(token);
                    if (*clean)
                        dapush(task->tags, strdup(clean));
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_status_regex)) !=
                   NULL) {
            free(task->status);
            task->status = strdup(trim(value));
            free(value);
        }
    }

    free(line);
    fclose(f);

    if (dalen(task->tags) == 0)
        dapush(task->tags, strdup(""));

    return task;
}

static Task **tasks_get_all(const char *main_dir) {
    DIR *dir = opendir(main_dir);
    if (!dir)
        return NULL;

    Task **tasks = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            !regex_match(entry->d_name, &g_task_dir_regex)) {
            continue;
        }

        char task_dir[PATH_MAX - 10];
        snprintf(task_dir, sizeof(task_dir), "%s/%s", main_dir, entry->d_name);

        char task_file[PATH_MAX];
        snprintf(task_file, sizeof(task_file), "%s/TASK.md", task_dir);

        if (!file_exists(task_file))
            continue;

        Task *task = task_parse(task_dir, entry->d_name);
        if (task)
            dapush(tasks, task);
    }

    closedir(dir);
    return tasks;
}

static void task_free(Task *task) {
    if (!task)
        return;

    free(task->path);
    free(task->name);
    free(task->status);
    free(task->time);
    for (int j = 0; j < dalen(task->tags); j++)
        free(task->tags[j]);
    dafree(task->tags);
    free(task);
}

static void tasks_free(Task **tasks) {
    if (!tasks)
        return;

    for (int i = 0; i < dalen(tasks); i++)
        task_free(tasks[i]);
    dafree(tasks);
}

static int task_matches_condition(Task *task, ASTNode *node) {
    if (!node)
        return 1;

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
        default:
            return 0;
        }

    case NODE_UNARY_OP:
        if (node->unary.op == OP_NOT)
            return !task_matches_condition(task, node->unary.expr);
        return 0;

    case NODE_COMPARISON: {
        switch (node->comparison.field) {
        case CMP_PRIORITY: {
            int task_val = task->priority;
            int cond_val = node->comparison.value.int_value;
            switch (node->comparison.cmp) {
            case CMP_GT:
                return task_val > cond_val;
            case CMP_LT:
                return task_val < cond_val;
            case CMP_GE:
                return task_val >= cond_val;
            case CMP_LE:
                return task_val <= cond_val;
            case CMP_EQ:
                return task_val == cond_val;
            case CMP_NE:
                return task_val != cond_val;
            default:
                return 0;
            }
        } // case CMP_PRIORITY

        case CMP_TAG: {
            char *cond_tag = node->comparison.value.str_value;
            for (int i = 0; i < dalen(task->tags); i++) {
                if (node->comparison.cmp == CMP_EQ) {
                    if (strcmp(task->tags[i], cond_tag) == 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_NE) {
                    if (strcmp(task->tags[i], cond_tag) == 0)
                        return 0;
                } else if (node->comparison.cmp == CMP_SUBSTR) {
                    if (strstr(task->tags[i], cond_tag) != NULL)
                        return 1;
                } else if (node->comparison.cmp == CMP_NSUBSTR) {
                    if (strstr(task->tags[i], cond_tag) != NULL)
                        return 0;
                } else if (node->comparison.cmp == CMP_GT) {
                    if (strcmp(task->tags[i], cond_tag) > 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_LT) {
                    if (strcmp(task->tags[i], cond_tag) < 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_GE) {
                    if (strcmp(task->tags[i], cond_tag) >= 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_LE) {
                    if (strcmp(task->tags[i], cond_tag) <= 0)
                        return 1;
                }
            }
            return (node->comparison.cmp == CMP_NE ||
                    node->comparison.cmp == CMP_NSUBSTR);
        } // case CMP_TAG

        case CMP_STATUS: {
            char *cond_status = node->comparison.value.str_value;
            if (!task->status)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(task->status, cond_status) == 0;
            case CMP_NE:
                return strcmp(task->status, cond_status) != 0;
            case CMP_SUBSTR:
                return strstr(task->status, cond_status) != NULL;
            case CMP_NSUBSTR:
                return strstr(task->status, cond_status) == NULL;
            case CMP_GT:
                return strcmp(task->status, cond_status) > 0;
            case CMP_LT:
                return strcmp(task->status, cond_status) < 0;
            case CMP_GE:
                return strcmp(task->status, cond_status) >= 0;
            case CMP_LE:
                return strcmp(task->status, cond_status) <= 0;
            default:
                return 0;
            }
        } // case CMP_STATUS

        case CMP_TIME: {
            char *cond_time = node->comparison.value.str_value;
            if (!task->time)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(task->time, cond_time) == 0;
            case CMP_NE:
                return strcmp(task->time, cond_time) != 0;
            case CMP_SUBSTR:
                return strstr(task->time, cond_time) != NULL;
            case CMP_NSUBSTR:
                return strstr(task->time, cond_time) == NULL;
            case CMP_GT:
                return strcmp(task->time, cond_time) > 0;
            case CMP_LT:
                return strcmp(task->time, cond_time) < 0;
            case CMP_GE:
                return strcmp(task->time, cond_time) >= 0;
            case CMP_LE:
                return strcmp(task->time, cond_time) <= 0;
            default:
                return 0;
            }
        } // case CMP_TIME

        case CMP_NAME: {
            char *cond_name = node->comparison.value.str_value;
            if (!task->name)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(task->name, cond_name) == 0;
            case CMP_NE:
                return strcmp(task->name, cond_name) != 0;
            case CMP_SUBSTR:
                return strstr(task->name, cond_name) != NULL;
            case CMP_NSUBSTR:
                return strstr(task->name, cond_name) == NULL;
            case CMP_GT:
                return strcmp(task->name, cond_name) > 0;
            case CMP_LT:
                return strcmp(task->name, cond_name) < 0;
            case CMP_GE:
                return strcmp(task->name, cond_name) >= 0;
            case CMP_LE:
                return strcmp(task->name, cond_name) <= 0;
            default:
                return 0;
            }
        } // case CMP_NAME

        default:
            return 0;
        } // switch (node->comparison.field)
    } // case NODE_COMPARISON

    default:
        return 0;
    }
}

static Task **tasks_filter(Task **tasks, ASTNode *filter) {
    if (!tasks || dalen(tasks) == 0 || !filter) {
        return tasks;
    }

    Task **filtered = NULL;
    for (int i = 0; i < dalen(tasks); i++) {
        Task *t = tasks[i];
        if (task_matches_condition(t, filter))
            dapush(filtered, t);
        else
            task_free(t);
    }

    dafree(tasks);
    return filtered;
}

typedef void (*task_operation_fn)(Task **tasks, void *ctx);

static void task_op_print(Task **tasks, void *ctx) {
    OutputFormat fmt = (OutputFormat)(intptr_t)ctx;
    if (!tasks)
        return;

    for (int i = 0; i < dalen(tasks); i++) {
        Task *t = tasks[i];

        switch (fmt) {
        case FMT_ONLY_PATH:
            printf("%s/TASK.md\n", t->path);
            break;
        case FMT_UNIX: // fallthrough
        default:
            printf("%s/TASK.md:1:1: STATUS:[%s] NAME:[%s] PRIORITY:[%d] TAGS:[",
                   t->path, t->status, t->name, t->priority);
            for (int j = 0; j < dalen(t->tags); j++) {
                printf("%s", t->tags[j]);
                if (j < dalen(t->tags) - 1)
                    printf(",");
            }
            printf("]\n");
            break;
        }
    }
}

static void task_op_delete(Task **tasks, void *ctx) {
    UNUSED(ctx);

    if (!tasks)
        return;

    for (int i = 0; i < dalen(tasks); i++) {
        Task *t = tasks[i];
        rmrf(t->path);
        printf("Removed: %s\n", t->path);
    }
}

static void tasks_process_with_filter(const char *query, task_operation_fn op,
                                      void *ctx) {
    // compile filter
    ASTNode *filter = parse(query);
    if (!filter)
        die("Failed to parse query: '%s'", query);

    // find main dir
    char *main_dir = find_dir_up(g_main_dir_name);
    if (!main_dir)
        die("Tasks directory not found");

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
    die("usage: %s [-h] [-i] [n] [-f format] [-p query] [-r query]\n"
        "  -h          show this help\n"
        "  -i          initialize main directory in current location\n"
        "  -n          create new task\n"
        "  -f format   output format for -p:\n"
        "                unix - path:1:1: STATUS:[...] NAME:[...] ... "
        "(default)\n"
        "                path - absolute paths only, one per line\n"
        "  -p query    print tasks using query (e.g. 'priority > 5')\n"
        "  -r query    remove tasks matching query",
        argv0);
}

int main(int argc, char **argv) {
    init_regexes();
    OutputFormat fmt = FMT_UNIX;
    char *query = NULL;
    int do_init = 0, do_new = 0, do_help = 0, do_remove = 0, do_print = 0;

    ARGBEGIN {
    case 'h': {
        do_help = 1;
        break;
    }
    case 'i': {
        do_init = 1;
        break;
    }
    case 'n': {
        do_new = 1;
        break;
    }
    case 'f': {
        char *fmt_str = ARGF();
        if (!fmt_str)
            die("-f requires a format argument (unix or path)");
        if (strcmp(fmt_str, "path") == 0)
            fmt = FMT_ONLY_PATH;
        else if (strcmp(fmt_str, "unix") == 0)
            fmt = FMT_UNIX;
        else
            die("Unknown format '%s'. Use 'unix' or 'path'", fmt_str);
        break;
    }
    case 'p': {
        query = ARGF();
        if (!query)
            die("-p requires a query argument");
        do_print = 1;
        break;
    }
    case 'r': {
        query = ARGF();
        if (!query)
            die("-r requires a query argument");
        do_remove = 1;
        break;
    }
    default:
        die("Unknown flag '%c'", ARGC());
    }
    ARGEND;

    if (do_help) {
        usage();
    } else if (do_init) {
        tasks_dir_init();
    } else if (do_new) {
        char *main_dir = find_dir_up(g_main_dir_name);
        if (!main_dir)
            die("Tasks directory not found");
        task_create_dir_and_md(main_dir);
        free(main_dir);
    } else if (do_print) {
        tasks_process_with_filter(query, task_op_print, (void *)(intptr_t)fmt);
    } else if (do_remove) {
        tasks_process_with_filter(query, task_op_delete, NULL);
    } else {
        usage();
    }

    free_regexes();
    return 0;
}
