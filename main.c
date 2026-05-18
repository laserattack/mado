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

static int cmp_string(const char *a, const char *b, ComparisonOperator op) {
    switch (op) {
    case CMP_EQ:
        return strcmp(a, b) == 0;
    case CMP_NE:
        return strcmp(a, b) != 0;
    case CMP_SUBSTR:
        return (strstr(a, b) != NULL);
    case CMP_NSUBSTR:
        return !(strstr(a, b) != NULL);
    case CMP_GT:
        return strcmp(a, b) > 0;
    case CMP_LT:
        return strcmp(a, b) < 0;
    case CMP_GE:
        return strcmp(a, b) >= 0;
    case CMP_LE:
        return strcmp(a, b) <= 0;
    default:
        return 0;
    }
}

static int cmp_int(int a, int b, ComparisonOperator op) {
    switch (op) {
    case CMP_EQ:
        return a == b;
    case CMP_NE:
        return a != b;
    case CMP_GT:
        return a > b;
    case CMP_LT:
        return a < b;
    case CMP_GE:
        return a >= b;
    case CMP_LE:
        return a <= b;
    default:
        return 0;
    }
}

// returns 1 if operator is "negative" (NE, NSUBSTR)
static int is_negated(ComparisonOperator op) {
    return op == CMP_NE || op == CMP_NSUBSTR;
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

    case NODE_LIST_COMPARISON: {
        ComparisonOperator op = node->list_comparison.cmp;

        // priority list
        if (node->list_comparison.field == CMP_PRIORITY) {
            NumList *list = node->list_comparison.num_list;
            for (int i = 0; i < list->count; i++) {
                if (is_negated(op)) {
                    if (task->priority == list->items[i])
                        return 0;
                } else {
                    if (cmp_int(task->priority, list->items[i], op))
                        return 1;
                }
            }
            return is_negated(op) ? 1 : 0;
        }

        StringList *list = node->list_comparison.str_list;

        // multiple string field
        if (node->list_comparison.field == CMP_TAG) {
            for (int j = 0; j < dalen(task->tags); j++) {
                for (int i = 0; i < list->count; i++) {
                    if (is_negated(op)) {
                        if (cmp_string(task->tags[j], list->items[i], op))
                            return 0;
                    } else {
                        if (cmp_string(task->tags[j], list->items[i], op))
                            return 1;
                    }
                }
            }
            return is_negated(op) ? 1 : 0;
        }

        // single string field
        char *tv = NULL;
        if (node->list_comparison.field == CMP_STATUS)
            tv = task->status;
        else if (node->list_comparison.field == CMP_NAME)
            tv = task->name;
        else if (node->list_comparison.field == CMP_TIME)
            tv = task->time;

        for (int i = 0; i < list->count; i++)
            if (cmp_string(tv, list->items[i], op))
                return is_negated(op) ? 0 : 1;

        return is_negated(op) ? 1 : 0;
    } // NODE_LIST_COMPARISON

    case NODE_COMPARISON: {
        ComparisonField field = node->comparison.field;
        ComparisonOperator op = node->comparison.cmp;

        if (field == CMP_PRIORITY)
            return cmp_int(task->priority, node->comparison.value.int_value,
                           op);

        if (field == CMP_TAG) {
            char *cv = node->comparison.value.str_value;
            for (int i = 0; i < dalen(task->tags); i++)
                if (cmp_string(task->tags[i], cv, op))
                    return is_negated(op) ? 0 : 1;
            return is_negated(op) ? 1 : 0;
        }

        char *tv = NULL;
        if (field == CMP_STATUS)
            tv = task->status;
        else if (field == CMP_NAME)
            tv = task->name;
        else if (field == CMP_TIME)
            tv = task->time;
        if (!tv)
            return 0;

        return cmp_string(tv, node->comparison.value.str_value, op);
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
    UNUSED(ctx);
    if (!tasks)
        return;

    for (int i = 0; i < dalen(tasks); i++) {
        Task *t = tasks[i];

        printf("%s/TASK.md:1:1: STATUS:[%s] NAME:[%s] PRIORITY:[%d] TAGS:[",
               t->path, t->status, t->name, t->priority);

        for (int j = 0; j < dalen(t->tags); j++) {
            printf("%s", t->tags[j]);
            if (j < dalen(t->tags) - 1)
                printf(",");
        }
        printf("]\n");
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
        if (!main_dir)
            die("Tasks directory not found");
        task_create_dir_and_md(main_dir);
        free(main_dir);
        goto cleanup;
    }
    case 'p': {
        char *query = ARGF();
        if (!query)
            die("-p requires a query argument");
        tasks_process_with_filter(query, task_op_print, NULL);
        goto cleanup;
    }
    case 'r': {
        char *query = ARGF();
        if (!query)
            die("-r requires a query argument");
        tasks_process_with_filter(query, task_op_delete, NULL);
        goto cleanup;
    }
    default: {
        die("Unknown flag '%c'", ARGC());
    }
    }
    ARGEND;

    usage();

cleanup:
    free_regexes();
    return 0;
}
