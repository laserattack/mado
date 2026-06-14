#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils/da.h"

#define FS_IMPL
#include "utils/fs.h"

#define UTIL_IMPL
#include "utils/util.h"

#define REGEX_IMPL
#include "utils/regex.h"

#include "ast.h"

#define UNUSED(x) (void)(x)

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// ================ TYPES

typedef enum { ERR_SUCCESS = 0,
               ERR_FAILURE } Error;

typedef enum {
    FMT_UNIX,
    FMT_ONLY_PATH,
    FMT_JSONL,
} Output_Format;

typedef struct Entry {
    char *path;
    char *name;
    uint16_t priority;
    char **tags;
    char *status;
    char *time;
    char *deadline;
} Entry;

typedef enum {
    FIELD_NONE = 0,
    FIELD_NAME = 1 << 0,
    FIELD_TIME = 1 << 1,
    FIELD_DEADLINE = 1 << 2,
    FIELD_PRIORITY = 1 << 3,
    FIELD_STATUS = 1 << 4,
    FIELD_TAGS = 1 << 5,
    FIELD_PATH = 1 << 6,
    FIELD_ALL = FIELD_NAME | FIELD_TIME | FIELD_DEADLINE | FIELD_PRIORITY |
                FIELD_STATUS | FIELD_TAGS | FIELD_PATH
} Entry_Field;

// ================ GLOBAL

static char *argv0;

#define MAIN_DIR_NAME_DEFAULT "MADO"
#define TEMPLATES_DIR_NAME_DEFAULT ".templates"
#define ENTRY_FILE_NAME_DEFAULT "MAIN"
#define TEMPLATE_NAME_DEFAULT "task"
#define MAX_HEADER_LINES_DEFAULT 30
#define MAX_HEADER_LINE_LEN_DEFAULT 1024

static struct {
    const char *main_dir_name;
    const char *templates_dir_name;
    const char *entry_file_name;
    const char *template_name;
    int max_header_lines;
    int max_header_line_len;
    Entry_Field hide_fields;
} g_config = {
    .main_dir_name = MAIN_DIR_NAME_DEFAULT,
    .templates_dir_name = TEMPLATES_DIR_NAME_DEFAULT,
    .entry_file_name = ENTRY_FILE_NAME_DEFAULT,
    .template_name = TEMPLATE_NAME_DEFAULT,
    .max_header_lines = MAX_HEADER_LINES_DEFAULT,
    .max_header_line_len = MAX_HEADER_LINE_LEN_DEFAULT,
    .hide_fields = FIELD_NONE,
};

static regex_t g_entry_dir_regex;
static regex_t g_name_regex;
static regex_t g_priority_regex;
static regex_t g_tags_regex;
static regex_t g_status_regex;
static regex_t g_deadline_regex;

static int g_regex_initialized = 0;

static Error init_regexes() {
    if (g_regex_initialized)
        return ERR_SUCCESS;

    if (regcomp(&g_entry_dir_regex, "^[0-9]{8}T[0-9]{6}$", REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: failed to compile entry_dir regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_name_regex, "^- NAME:[[:space:]]*(.*)$", REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: failed to compile name regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_priority_regex, "^- PRIORITY:[[:space:]]*([0-9]{1,3})$",
                REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: failed to compile priority regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_tags_regex, "^- TAGS:[[:space:]]*(.*)$", REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: failed to compile tags regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_status_regex, "^- STATUS:[[:space:]]*(.*)$", REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: failed to compile status regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_deadline_regex,
                "^- DEADLINE:[[:space:]]*([0-9]{4}|[0-9]{6}|[0-9]{8}(T([0-9]{2}|[0-9]{4}|[0-9]{6})?)?)[[:space:]]*$",
                REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: failed to compile deadline regex\n");
        return ERR_FAILURE;
    }

    g_regex_initialized = 1;
    return ERR_SUCCESS;
}

static void free_regexes() {
    if (!g_regex_initialized)
        return;
    regfree(&g_entry_dir_regex);
    regfree(&g_name_regex);
    regfree(&g_priority_regex);
    regfree(&g_tags_regex);
    regfree(&g_status_regex);
    regfree(&g_deadline_regex);
}

// ================ INTERPRETER

static void print_json_string(const char *str) {
    putchar('"');
    for (const char *p = str; *p; p++) {
        switch (*p) {
        case '"':
            printf("\\\"");
            break;
        case '\\':
            printf("\\\\");
            break;
        default:
            putchar(*p);
        }
    }
    putchar('"');
}

static void entry_print(Entry *e, Output_Format fmt) {
    Entry_Field hidden = g_config.hide_fields;
    Entry_Field shown = FIELD_ALL & ~hidden;

    if (fmt == FMT_ONLY_PATH) {
        printf("%s/%s.md\n", e->path, g_config.entry_file_name);
        return;
    }

    if (fmt == FMT_JSONL) {
        printf("{");
        int has_any = 0;
        if (shown & FIELD_TIME) {
            if (has_any)
                printf(",");
            printf("\"time\":");
            print_json_string(e->time);
            has_any = 1;
        }
        if (shown & FIELD_NAME) {
            if (has_any)
                printf(",");
            printf("\"name\":");
            print_json_string(e->name);
            has_any = 1;
        }
        if (shown & FIELD_PRIORITY) {
            if (has_any)
                printf(",");
            printf("\"priority\":%d", e->priority);
            has_any = 1;
        }
        if (shown & FIELD_DEADLINE) {
            if (has_any)
                printf(",");
            printf("\"deadline\":");
            print_json_string(e->deadline);
            has_any = 1;
        }
        if (shown & FIELD_STATUS) {
            if (has_any)
                printf(",");
            printf("\"status\":");
            print_json_string(e->status);
            has_any = 1;
        }
        if (shown & FIELD_TAGS) {
            if (has_any)
                printf(",");
            printf("\"tags\":[");
            int first_tag = 1;
            for (int j = 0; j < dalen(e->tags); j++) {
                if (strcmp(e->tags[j], "") == 0)
                    continue;
                if (!first_tag)
                    printf(",");
                print_json_string(e->tags[j]);
                first_tag = 0;
            }
            printf("]");
            has_any = 1;
        }
        if (shown & FIELD_PATH) {
            if (has_any)
                printf(",");
            printf("\"path\":\"%s/%s.md\"", e->path, g_config.entry_file_name);
            has_any = 1;
        }
        printf("}\n");
        return;
    }

    if (fmt == FMT_UNIX) {
        printf("%s/%s.md:1:1:", e->path, g_config.entry_file_name);
        if (shown & FIELD_TIME)
            printf(" TIME:[%s]", e->time);
        if (shown & FIELD_NAME)
            printf(" NAME:[%s]", e->name);
        if (shown & FIELD_PRIORITY)
            printf(" PRIORITY:[%d]", e->priority);
        if (shown & FIELD_DEADLINE)
            printf(" DEADLINE:[%s]", e->deadline);
        if (shown & FIELD_STATUS)
            printf(" STATUS:[%s]", e->status);
        if (shown & FIELD_TAGS) {
            printf(" TAGS:[");
            for (int j = 0; j < dalen(e->tags); j++) {
                if (strcmp(e->tags[j], "") == 0)
                    continue;
                printf("%s", e->tags[j]);
                if (j < dalen(e->tags) - 1 && strcmp(e->tags[j + 1], "") != 0)
                    printf(",");
            }
            printf("]");
        }
        printf("\n");
    }
}

static Error templates_dir_init() {
    char *main_dir = find_dir_up(g_config.main_dir_name);
    if (!main_dir) {
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_config.main_dir_name);
        return ERR_FAILURE;
    }

    char templates_dir[PATH_MAX - 15];
    snprintf(templates_dir, sizeof(templates_dir), "%s/%s", main_dir, g_config.templates_dir_name);

    if (!file_exists(templates_dir)) {
        if (mkdir(templates_dir, 0755) != 0) {
            free(main_dir);
            fprintf(stderr, "Error: failed to create templates directory: %s\n", templates_dir);
            return ERR_FAILURE;
        }
        printf("Created templates directory: %s\n", templates_dir);
    } else {
        printf("Templates directory already exists: %s\n", templates_dir);
    }

    char task_template[PATH_MAX];
    snprintf(task_template, sizeof(task_template), "%s/task.md", templates_dir);
    if (!file_exists(task_template)) {
        FILE *f = fopen(task_template, "w");
        if (f) {
            fprintf(f, "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n");
            fclose(f);
            printf("Created task template: %s\n", task_template);
        } else {
            free(main_dir);
            fprintf(stderr, "Error: failed to create task template: %s\n", task_template);
            return ERR_FAILURE;
        }
    } else {
        printf("Task template already exists: %s\n", task_template);
    }

    char note_template[PATH_MAX];
    snprintf(note_template, sizeof(note_template), "%s/note.md", templates_dir);
    if (!file_exists(note_template)) {
        FILE *f = fopen(note_template, "w");
        if (f) {
            fprintf(f, "- NAME:\n- TAGS:\n");
            fclose(f);
            printf("Created note template: %s\n", note_template);
        } else {
            free(main_dir);
            fprintf(stderr, "Error: failed to create note template: %s\n", note_template);
            return ERR_FAILURE;
        }
    } else {
        printf("Note template already exists: %s\n", note_template);
    }

    free(main_dir);
    return ERR_SUCCESS;
}

static Error entries_dir_init(int force) {
    char *cwd = get_cwd();
    char entries_dir[PATH_MAX];
    snprintf(entries_dir, sizeof(entries_dir), "%s/%s", cwd, g_config.main_dir_name);

    if (!force) {
        char *entries_dir_existing = find_dir_up(g_config.main_dir_name);
        if (entries_dir_existing) {
            printf("Entries directory already exists: %s\n", entries_dir_existing);
            if (strcmp(entries_dir_existing, entries_dir))
                printf("Hint: use -F to force initialization in current directory\n");
            free(entries_dir_existing);
            free(cwd);
            return ERR_SUCCESS;
        }
    }

    if (file_exists(entries_dir)) {
        printf("Entries directory already exists: %s\n", entries_dir);
        if (force)
            printf("Hint: -F has no effect because '%s' already exists in this location\n", g_config.main_dir_name);
    } else {
        if (mkdir(entries_dir, 0755) == 0) {
            printf("Created entries directory: %s\n", entries_dir);
        } else {
            free(cwd);
            fprintf(stderr, "Error: failed to create entries directory: %s\n", entries_dir);
            return ERR_FAILURE;
        }
    }

    free(cwd);
    return ERR_SUCCESS;
}

static Error entry_create(const char *entry_path, const char *template_name) {
    char entry_md[PATH_MAX];
    snprintf(entry_md, sizeof(entry_md), "%s/%s.md", entry_path, g_config.entry_file_name);

    if (template_name) {
        char template_file[PATH_MAX];
        char *main_dir = find_dir_up(g_config.main_dir_name);
        if (!main_dir) {
            fprintf(stderr, "Error: entries directory '%s' not found\n", g_config.main_dir_name);
            return ERR_FAILURE;
        }

        snprintf(template_file, sizeof(template_file), "%s/%s/%s.md", main_dir, g_config.templates_dir_name, template_name);
        free(main_dir);

        if (file_exists(template_file)) {
            FILE *src = fopen(template_file, "r");
            if (!src) {
                fprintf(stderr, "Error: failed to open template: %s\n", template_file);
                return ERR_FAILURE;
            }

            FILE *dst = fopen(entry_md, "w");
            if (!dst) {
                fclose(src);
                fprintf(stderr, "Error: failed to create %s.md: %s\n", g_config.entry_file_name, entry_md);
                return ERR_FAILURE;
            }

            char buffer[4096];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
                fwrite(buffer, 1, n, dst);

            fclose(src);
            fclose(dst);
            return ERR_SUCCESS;
        } else {
            fprintf(stderr, "Error: template '%s' was not found\n", template_name);
            return ERR_FAILURE;
        }
    }

    FILE *f = fopen(entry_md, "w");
    if (!f) {
        fprintf(stderr, "Error: failed to create %s.md in: %s\n", g_config.entry_file_name, entry_path);
        return ERR_FAILURE;
    }
    fclose(f);
    return ERR_SUCCESS;
}

static Error entry_create_dir_and_md(const char *main_dir, Output_Format fmt) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);

    char entry_path[PATH_MAX - 10];
    snprintf(entry_path, sizeof(entry_path), "%s/%s", main_dir, dir_name);

    if (file_exists(entry_path)) {
        fprintf(stderr, "Error: entry directory already exists\n");
        return ERR_FAILURE;
    }
    if (mkdir(entry_path, 0755) != 0) {
        fprintf(stderr, "Error: failed to create entry directory\n");
        return ERR_FAILURE;
    }

    if (entry_create(entry_path, g_config.template_name) != ERR_SUCCESS) {
        return ERR_FAILURE;
    }

    Entry entry = {.path = entry_path, .name = "", .status = "", .time = dir_name, .deadline = ""};
    entry_print(&entry, fmt);
    return ERR_SUCCESS;
}

static Entry *entry_parse(const char *entry_dir, const char *entry_time) {
    char entry_file[PATH_MAX];
    snprintf(entry_file, sizeof(entry_file), "%s/%s.md", entry_dir, g_config.entry_file_name);

    FILE *f = fopen(entry_file, "r");
    if (!f)
        return NULL;

    Entry *entry = calloc(1, sizeof(Entry));
    entry->priority = 0;
    entry->path = strdup(entry_dir);
    entry->status = strdup("");
    entry->name = strdup("");
    entry->time = strdup(entry_time);
    entry->deadline = strdup("99990000T000000");

    char *line = malloc(g_config.max_header_line_len);
    int lines_processed = 0;

    while (lines_processed < g_config.max_header_lines &&
           fgets(line, g_config.max_header_line_len, f)) {
        lines_processed++;
        size_t len = strlen(line);

        if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
            int c;
            while ((c = fgetc(f)) != '\n' && c != EOF)
                ;
        }
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        char *value;
        if ((value = regex_extract_first_group(line, &g_name_regex)) != NULL) {
            free(entry->name);
            entry->name = strdup(trim(value));
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_priority_regex)) != NULL) {
            entry->priority = atoi(value);
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_tags_regex)) != NULL) {
            char *tags_str = trim(value);
            if (tags_str && *tags_str) {
                char *saveptr;
                char *token = strtok_r(tags_str, ",", &saveptr);
                while (token) {
                    char *clean = trim(token);
                    if (*clean)
                        dapush(entry->tags, strdup(clean));
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_status_regex)) != NULL) {
            free(entry->status);
            entry->status = strdup(trim(value));
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_deadline_regex)) != NULL) {
            free(entry->deadline);
            entry->deadline = strdup(trim(value));
            free(value);
        }
    }

    free(line);
    fclose(f);

    if (dalen(entry->tags) == 0)
        dapush(entry->tags, strdup(""));

    return entry;
}

static Entry **entries_get_all(const char *main_dir) {
    DIR *dir = opendir(main_dir);
    if (!dir)
        return NULL;

    Entry **entries = NULL;
    struct dirent *dirent;

    while ((dirent = readdir(dir)) != NULL) {
        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0 ||
            !regex_match(dirent->d_name, &g_entry_dir_regex)) {
            continue;
        }

        char entry_dir[PATH_MAX - 10];
        snprintf(entry_dir, sizeof(entry_dir), "%s/%s", main_dir, dirent->d_name);

        char entry_file[PATH_MAX];
        snprintf(entry_file, sizeof(entry_file), "%s/%s.md", entry_dir, g_config.entry_file_name);

        if (!file_exists(entry_file))
            continue;

        Entry *entry = entry_parse(entry_dir, dirent->d_name);
        if (entry)
            dapush(entries, entry);
    }

    closedir(dir);
    return entries;
}

static void entry_free(Entry *entry) {
    if (!entry)
        return;
    free(entry->path);
    free(entry->name);
    free(entry->status);
    free(entry->time);
    free(entry->deadline);
    for (int j = 0; j < dalen(entry->tags); j++)
        free(entry->tags[j]);
    dafree(entry->tags);
    free(entry);
}

static void entries_free(Entry **entries) {
    if (!entries)
        return;
    for (int i = 0; i < dalen(entries); i++)
        entry_free(entries[i]);
    dafree(entries);
}

static int entry_matches_condition(Entry *entry, ASTNode *node) {
    if (!node)
        return 1;

    switch (node->type) {
    case NODE_ALL:
        return 1;
    case NODE_BINARY_OP:
        switch (node->binary.op) {
        case OP_AND:
            return entry_matches_condition(entry, node->binary.left) && entry_matches_condition(entry, node->binary.right);
        case OP_OR:
            return entry_matches_condition(entry, node->binary.left) || entry_matches_condition(entry, node->binary.right);
        default:
            return 0;
        }
    case NODE_UNARY_OP:
        if (node->unary.op == OP_NOT)
            return !entry_matches_condition(entry, node->unary.expr);
        return 0;
    case NODE_COMPARISON: {
        switch (node->comparison.field) {
        case CMP_PRIORITY: {
            int entry_val = entry->priority;
            int cond_val = node->comparison.value.int_value;
            switch (node->comparison.cmp) {
            case CMP_GT:
                return entry_val > cond_val;
            case CMP_LT:
                return entry_val < cond_val;
            case CMP_GE:
                return entry_val >= cond_val;
            case CMP_LE:
                return entry_val <= cond_val;
            case CMP_TILDE:
            case CMP_EQ:
                return entry_val == cond_val;
            case CMP_NTILDE:
            case CMP_NE:
                return entry_val != cond_val;
            default:
                return 0;
            }
        }
        case CMP_TAG: {
            char *cond_tag = node->comparison.value.str_value;
            for (int i = 0; i < dalen(entry->tags); i++) {
                switch (node->comparison.cmp) {
                case CMP_EQ:
                    if (strcmp(entry->tags[i], cond_tag) == 0)
                        return 1;
                    break;
                case CMP_NE:
                    if (strcmp(entry->tags[i], cond_tag) == 0)
                        return 0;
                    break;
                case CMP_TILDE:
                    if (strstr(entry->tags[i], cond_tag) != NULL)
                        return 1;
                    break;
                case CMP_NTILDE:
                    if (strstr(entry->tags[i], cond_tag) != NULL)
                        return 0;
                    break;
                case CMP_GT:
                    if (strcmp(entry->tags[i], cond_tag) > 0)
                        return 1;
                    break;
                case CMP_LT:
                    if (strcmp(entry->tags[i], cond_tag) < 0)
                        return 1;
                    break;
                case CMP_GE:
                    if (strcmp(entry->tags[i], cond_tag) >= 0)
                        return 1;
                    break;
                case CMP_LE:
                    if (strcmp(entry->tags[i], cond_tag) <= 0)
                        return 1;
                    break;
                default:
                    break;
                }
            }
            return (node->comparison.cmp == CMP_NE || node->comparison.cmp == CMP_NTILDE);
        }
        case CMP_STATUS: {
            char *cond = node->comparison.value.str_value;
            if (!entry->status)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->status, cond) == 0;
            case CMP_NE:
                return strcmp(entry->status, cond) != 0;
            case CMP_TILDE:
                return strstr(entry->status, cond) != NULL;
            case CMP_NTILDE:
                return strstr(entry->status, cond) == NULL;
            case CMP_GT:
                return strcmp(entry->status, cond) > 0;
            case CMP_LT:
                return strcmp(entry->status, cond) < 0;
            case CMP_GE:
                return strcmp(entry->status, cond) >= 0;
            case CMP_LE:
                return strcmp(entry->status, cond) <= 0;
            default:
                return 0;
            }
        }
        case CMP_DEADLINE: {
            char *cond = node->comparison.value.str_value;
            if (!entry->deadline)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->deadline, cond) == 0;
            case CMP_NE:
                return strcmp(entry->deadline, cond) != 0;
            case CMP_TILDE:
                return strstr(entry->deadline, cond) != NULL;
            case CMP_NTILDE:
                return strstr(entry->deadline, cond) == NULL;
            case CMP_GT:
                return strcmp(entry->deadline, cond) > 0;
            case CMP_LT:
                return strcmp(entry->deadline, cond) < 0;
            case CMP_GE:
                return strcmp(entry->deadline, cond) >= 0;
            case CMP_LE:
                return strcmp(entry->deadline, cond) <= 0;
            default:
                return 0;
            }
        }
        case CMP_TIME: {
            char *cond = node->comparison.value.str_value;
            if (!entry->time)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->time, cond) == 0;
            case CMP_NE:
                return strcmp(entry->time, cond) != 0;
            case CMP_TILDE:
                return strstr(entry->time, cond) != NULL;
            case CMP_NTILDE:
                return strstr(entry->time, cond) == NULL;
            case CMP_GT:
                return strcmp(entry->time, cond) > 0;
            case CMP_LT:
                return strcmp(entry->time, cond) < 0;
            case CMP_GE:
                return strcmp(entry->time, cond) >= 0;
            case CMP_LE:
                return strcmp(entry->time, cond) <= 0;
            default:
                return 0;
            }
        }
        case CMP_NAME: {
            char *cond = node->comparison.value.str_value;
            if (!entry->name)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->name, cond) == 0;
            case CMP_NE:
                return strcmp(entry->name, cond) != 0;
            case CMP_TILDE:
                return strstr(entry->name, cond) != NULL;
            case CMP_NTILDE:
                return strstr(entry->name, cond) == NULL;
            case CMP_GT:
                return strcmp(entry->name, cond) > 0;
            case CMP_LT:
                return strcmp(entry->name, cond) < 0;
            case CMP_GE:
                return strcmp(entry->name, cond) >= 0;
            case CMP_LE:
                return strcmp(entry->name, cond) <= 0;
            default:
                return 0;
            }
        }
        default:
            return 0;
        }
    }
    default:
        return 0;
    }
}

static Entry **entries_filter(Entry **entries, ASTNode *filter) {
    if (!entries || dalen(entries) == 0 || !filter)
        return entries;
    Entry **filtered = NULL;
    for (int i = 0; i < dalen(entries); i++) {
        Entry *e = entries[i];
        if (entry_matches_condition(e, filter))
            dapush(filtered, e);
        else
            entry_free(e);
    }
    dafree(entries);
    return filtered;
}

typedef void (*entry_operation_fn)(Entry **entries, void *ctx);

static void entry_op_print(Entry **entries, void *ctx) {
    Output_Format fmt = (Output_Format)(intptr_t)ctx;
    if (!entries)
        return;
    for (int i = 0; i < dalen(entries); i++)
        entry_print(entries[i], fmt);
}

static void entry_op_delete(Entry **entries, void *ctx) {
    UNUSED(ctx);
    if (!entries)
        return;
    for (int i = 0; i < dalen(entries); i++) {
        Entry *e = entries[i];
        rmrf(e->path);
        printf("Removed: %s\n", e->path);
    }
}

static Error entries_process_with_filter(const char *query, entry_operation_fn op, void *ctx) {
    ASTNode *filter = NULL;
    if (query) {
        filter = parse(query);
        if (!filter) {
            fprintf(stderr, "Error: failed to parse query\n");
            return ERR_FAILURE;
        }
    }

    char *main_dir = find_dir_up(g_config.main_dir_name);
    if (!main_dir) {
        ast_free(filter);
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_config.main_dir_name);
        return ERR_FAILURE;
    }

    Entry **entries = entries_get_all(main_dir);
    entries = entries_filter(entries, filter);
    op(entries, ctx);
    entries_free(entries);
    free(main_dir);
    ast_free(filter);
    return ERR_SUCCESS;
}

static Error print_repo_info() {
    char *main_dir = find_dir_up(g_config.main_dir_name);
    if (!main_dir) {
        printf("No mado repository here\n");
        return ERR_SUCCESS;
    }
    Entry **entries = entries_get_all(main_dir);
    int count = entries ? dalen(entries) : 0;
    printf("Main directory: %s\n", main_dir);
    printf("Entries count:  %d\n", count);
    entries_free(entries);
    free(main_dir);
    return ERR_SUCCESS;
}

static int parse_format(const char *format_str, Output_Format *fmt) {
    if (strcmp(format_str, "path") == 0)
        *fmt = FMT_ONLY_PATH;
    else if (strcmp(format_str, "unix") == 0)
        *fmt = FMT_UNIX;
    else if (strcmp(format_str, "jsonl") == 0)
        *fmt = FMT_JSONL;
    else
        return 0;
    return 1;
}

// ================ COMMAND SYSTEM

typedef struct {
    const char *name;
    int has_arg;
    int *flag;
    int val;
    const char *description;
} Mado_Option;

typedef Error (*command_handler)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *description;
    const char *usage;
    const Mado_Option *options;
    command_handler handler;
} Command;

static Error cmd_init(int argc, char **argv);
static Error cmd_new(int argc, char **argv);
static Error cmd_list(int argc, char **argv);
static Error cmd_remove(int argc, char **argv);
static Error cmd_info(int argc, char **argv);
static Error cmd_help(int argc, char **argv);

static Command commands[] = {
    {"init",
     "Initialize mado repository",
     "mado init [COMMAND OPTIONS]",
     (Mado_Option[]){
         {"force", no_argument, NULL, 'F', "Force init in current directory"},
         {"main-dir", required_argument, NULL, 'D', "Custom main directory name"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_init},

    {"new",
     "Create new entry",
     "mado new [COMMAND OPTIONS]",
     (Mado_Option[]){
         {"template", required_argument, NULL, 't', "Template to use (task, note)"},
         {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_new},

    {"list",
     "List entries with optional filtering",
     "mado list [COMMAND OPTIONS] <QUERY>",
     (Mado_Option[]){
         {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"},
         {"hide-name", no_argument, NULL, 'N', "Hide name field"},
         {"hide-time", no_argument, NULL, 'T', "Hide time field"},
         {"hide-deadline", no_argument, NULL, 'I', "Hide deadline field"},
         {"hide-priority", no_argument, NULL, 'P', "Hide priority field"},
         {"hide-status", no_argument, NULL, 'S', "Hide status field"},
         {"hide-tags", no_argument, NULL, 'A', "Hide tags field"},
         {"hide-path", no_argument, NULL, 'H', "Hide path field"},
         {"only-hidden", no_argument, NULL, 'o', "Show only hidden fields"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_list},

    {"remove",
     "Remove entries matching query",
     "mado remove <QUERY>",
     NULL,
     cmd_remove},

    {"info",
     "Show repository information",
     "mado info",
     NULL,
     cmd_info},

    {"help",
     "Show help for commands",
     "mado help [COMMAND]",
     NULL,
     cmd_help},

    {NULL, NULL, NULL, NULL, NULL}};

static struct option *mado_option_to_getopt(const Mado_Option *opts, char **short_str) {
    int n = 0;
    while (opts[n].name)
        n++;

    struct option *gopts = malloc((n + 1) * sizeof(struct option));
    size_t ssize = 32;
    char *sstr = malloc(ssize);
    sstr[0] = '\0';

    for (int i = 0; i < n; i++) {
        gopts[i].name = opts[i].name;
        gopts[i].has_arg = opts[i].has_arg;
        gopts[i].flag = opts[i].flag;
        gopts[i].val = opts[i].val;

        if (opts[i].val && isprint(opts[i].val)) {
            size_t len = strlen(sstr);
            if (len + 3 >= ssize) {
                ssize *= 2;
                sstr = realloc(sstr, ssize);
            }
            sstr[len] = opts[i].val;
            if (opts[i].has_arg == required_argument) {
                sstr[len + 1] = ':';
                sstr[len + 2] = '\0';
            } else if (opts[i].has_arg == optional_argument) {
                sstr[len + 1] = ':';
                sstr[len + 2] = ':';
                sstr[len + 3] = '\0';
            } else {
                sstr[len + 1] = '\0';
            }
        }
    }
    gopts[n] = (struct option){0, 0, 0, 0};
    *short_str = sstr;
    return gopts;
}

static char *mado_options_help(const Mado_Option *opts) {
    size_t size = 256;
    char *buf = malloc(size);
    buf[0] = '\0';

    for (int i = 0; opts[i].name; i++) {
        char line[128];
        if (opts[i].val && isprint(opts[i].val)) {
            snprintf(line, sizeof(line), "  -%c, --%-16s %s\n",
                     opts[i].val, opts[i].name, opts[i].description);
        } else {
            snprintf(line, sizeof(line), "  --%-20s %s\n",
                     opts[i].name, opts[i].description);
        }
        size_t needed = strlen(buf) + strlen(line) + 1;
        if (needed > size) {
            size *= 2;
            buf = realloc(buf, size);
        }
        strcat(buf, line);
    }
    return buf;
}

static void print_command_help(const Command *cmd) {
    fprintf(stdout, "Usage: %s\n\n", cmd->usage);
    fprintf(stdout, "%s\n", cmd->description);
    if (cmd->options) {
        fprintf(stdout, "\nOptions:\n");
        char *help = mado_options_help(cmd->options);
        fprintf(stdout, "%s", help);
        free(help);
    }
}

static Command *find_command(const char *name) {
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
    }
    return NULL;
}

static void print_usage() {
    fprintf(stdout, "Usage: %s [GLOBAL FLAGS] <command> [COMMAND OPTIONS]\n\n", argv0);
    fprintf(stdout, "Global flags:\n");
    fprintf(stdout, "  -C, --change-working-dir <DIR>   Change working directory before command\n");
    fprintf(stdout, "  -h, --help                       Show this help\n");
    fprintf(stdout, "\nCommands:\n");
    for (int i = 0; commands[i].name; i++)
        fprintf(stdout, "  %-12s %s\n", commands[i].name, commands[i].description);
    fprintf(stdout, "\nRun '%s help <command>' for more information on a command.\n", argv0);
}

// ================ COMMAND HANDLERS

static Error cmd_init(int argc, char **argv) {
    int force = 0;
    const Command *cmd = find_command(argv[0]);
    char *short_str;
    struct option *gopts = mado_option_to_getopt(cmd->options, &short_str);

    int opt;
    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        switch (opt) {
        case 'F':
            force = 1;
            break;
        case 'D':
            g_config.main_dir_name = optarg;
            break;
        default:
            free(short_str);
            free(gopts);
            return ERR_FAILURE;
        }
    }
    free(short_str);
    free(gopts);

    Error err = entries_dir_init(force);
    if (err != ERR_SUCCESS)
        return err;
    return templates_dir_init();
}

static Error cmd_new(int argc, char **argv) {
    Output_Format fmt = FMT_UNIX;
    const Command *cmd = find_command(argv[0]);
    char *short_str;
    struct option *gopts = mado_option_to_getopt(cmd->options, &short_str);

    int opt;
    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        switch (opt) {
        case 't':
            g_config.template_name = optarg;
            break;
        case 'f':
            if (!parse_format(optarg, &fmt)) {
                fprintf(stderr, "Error: unknown format '%s'\n", optarg);
                free(short_str);
                free(gopts);
                return ERR_FAILURE;
            }
            break;
        default:
            free(short_str);
            free(gopts);
            return ERR_FAILURE;
        }
    }
    free(short_str);
    free(gopts);

    char *main_dir = find_dir_up(g_config.main_dir_name);
    if (!main_dir) {
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_config.main_dir_name);
        return ERR_FAILURE;
    }
    Error ret = entry_create_dir_and_md(main_dir, fmt);
    free(main_dir);
    return ret;
}

static Error cmd_list(int argc, char **argv) {
    char *query = NULL;
    Output_Format fmt = FMT_UNIX;
    const Command *cmd = find_command(argv[0]);
    char *short_str;
    struct option *gopts = mado_option_to_getopt(cmd->options, &short_str);

    int only = 0;
    int opt;

    opterr = 0;
    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        if (opt == 'o') {
            only = 1;
            g_config.hide_fields = FIELD_ALL;
        }
    }

    optind = 1;
    opterr = 1;

    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        switch (opt) {
        case 'f':
            if (!parse_format(optarg, &fmt)) {
                free(short_str);
                free(gopts);
                return ERR_FAILURE;
            }
            break;
        case 'o':
            break;
        case 'N':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_NAME) : (g_config.hide_fields | FIELD_NAME);
            break;
        case 'T':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_TIME) : (g_config.hide_fields | FIELD_TIME);
            break;
        case 'I':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_DEADLINE) : (g_config.hide_fields | FIELD_DEADLINE);
            break;
        case 'P':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_PRIORITY) : (g_config.hide_fields | FIELD_PRIORITY);
            break;
        case 'S':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_STATUS) : (g_config.hide_fields | FIELD_STATUS);
            break;
        case 'A':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_TAGS) : (g_config.hide_fields | FIELD_TAGS);
            break;
        case 'H':
            g_config.hide_fields = only ? (g_config.hide_fields & ~FIELD_PATH) : (g_config.hide_fields | FIELD_PATH);
            break;
        default:
            free(short_str);
            free(gopts);
            return ERR_FAILURE;
        }
    }
    free(short_str);
    free(gopts);

    if (optind < argc)
        query = argv[optind];
    return entries_process_with_filter(query, entry_op_print, (void *)(intptr_t)fmt);
}

static Error cmd_remove(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: remove requires a query argument\n");
        return ERR_FAILURE;
    }
    return entries_process_with_filter(argv[1], entry_op_delete, NULL);
}

static Error cmd_info(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    return print_repo_info();
}

static Error cmd_help(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return ERR_SUCCESS;
    }
    Command *cmd = find_command(argv[1]);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        print_usage();
        return ERR_FAILURE;
    }
    print_command_help(cmd);
    return ERR_SUCCESS;
}

// ================ GLOBAL FLAGS

static int handle_global_flags(int argc, char **argv) {
    static struct option global_options[] = {
        {"change-working-dir", required_argument, 0, 'C'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "+C:h", global_options, NULL)) != -1) {
        switch (opt) {
        case 'C':
            if (chdir(optarg) != 0) {
                fprintf(stderr, "Error: failed to change directory to '%s': %s\n", optarg, strerror(errno));
                return -1;
            }
            break;
        case 'h':
            print_usage();
            return 1;
        default:
            return -1;
        }
    }
    return 0;
}

// ================ ENTRYPOINT

int main(int argc, char **argv) {
    argv0 = argv[0];

    if (init_regexes() != ERR_SUCCESS)
        return ERR_FAILURE;
    atexit(free_regexes);

    if (argc < 2) {
        print_usage();
        return ERR_SUCCESS;
    }

    int ret = handle_global_flags(argc, argv);
    if (ret != 0)
        return ret < 0 ? ERR_FAILURE : ERR_SUCCESS;

    if (optind >= argc) {
        print_usage();
        return ERR_SUCCESS;
    }

    char *command_name = argv[optind];
    Command *cmd = find_command(command_name);

    if (!cmd) {
        fprintf(stderr, "Error: unknown command '%s'\n\n", command_name);
        print_usage();
        return ERR_FAILURE;
    }

    int cmd_argc = argc - optind;
    char **cmd_argv = argv + optind;
    optind = 1;

    return cmd->handler(cmd_argc, cmd_argv);
}
