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

typedef enum { ERR_SUCCESS = 0, ERR_FAILURE } Error;

typedef enum {
    FMT_UNIX,
    FMT_ONLY_PATH,
    FMT_JSONL,
} OutputFormat;

typedef struct Entry {
    char *path; // absolute path to entry dir
    char *name;
    uint16_t priority;
    char **tags; // dynarr
    char *status;
    char *time;
} Entry;

typedef enum {
    FIELD_NONE = 0,
    FIELD_NAME = 1 << 0,
    FIELD_TIME = 1 << 1,
    FIELD_PRIORITY = 1 << 2,
    FIELD_STATUS = 1 << 3,
    FIELD_TAGS = 1 << 4,
    FIELD_PATH = 1 << 5,
    FIELD_ALL = FIELD_NAME | FIELD_TIME | FIELD_PRIORITY | FIELD_STATUS |
                FIELD_TAGS | FIELD_PATH
} EntryField;

// ================ GLOBAL

// config
static struct {
    const char *main_dir_name;
    const char *templates_dir_name;
    const char *entry_file_name;
    const char *template_name;
    int max_header_lines;
    EntryField hide_fields;
} g_config = {
    .main_dir_name = "MADO",
    .templates_dir_name = ".templates",
    .entry_file_name = "MAIN.md",
    .template_name = "default", // default template
    .max_header_lines = 30,
    .hide_fields = FIELD_NONE,
};

// precompiled regexp for file parsing
static regex_t g_entry_dir_regex;
static regex_t g_name_regex;
static regex_t g_priority_regex;
static regex_t g_tags_regex;
static regex_t g_status_regex;

static int g_regex_initialized = 0;

static Error init_regexes(void) {
    if (g_regex_initialized)
        return ERR_SUCCESS;

    if (regcomp(&g_entry_dir_regex, "^[0-9]{8}T[0-9]{6}$", REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: Failed to compile entry_dir regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_name_regex, "^- NAME:[[:space:]]*(.*)$", REG_EXTENDED) !=
        0) {
        fprintf(stderr, "Error: Failed to compile name regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_priority_regex, "^- PRIORITY:[[:space:]]*([0-9]{1,3})$",
                REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: Failed to compile priority regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_tags_regex, "^- TAGS:[[:space:]]*(.*)$", REG_EXTENDED) !=
        0) {
        fprintf(stderr, "Error: Failed to compile tags regex\n");
        return ERR_FAILURE;
    }
    if (regcomp(&g_status_regex, "^- STATUS:[[:space:]]*(.*)$", REG_EXTENDED) !=
        0) {
        fprintf(stderr, "Error: Failed to compile status regex\n");
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

static void entry_print(Entry *e, OutputFormat fmt) {
    EntryField hidden = g_config.hide_fields;
    EntryField shown = FIELD_ALL & ~hidden;

    // FMT_ONLY_PATH
    if (fmt == FMT_ONLY_PATH) {
        if (shown & FIELD_PATH)
            printf("%s/%s\n", e->path, g_config.entry_file_name);
        return;
    }

    // FMT_JSONL
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
            printf("\"path\":\"%s/%s\"", e->path, g_config.entry_file_name);
            has_any = 1;
        }
        printf("}\n");
        return;
    }

    // FMT_UNIX
    if (fmt == FMT_UNIX) {
        int has_any = 0;

        if (shown & FIELD_PATH) {
            printf("%s/%s:1:1:", e->path, g_config.entry_file_name);
            has_any = 1;
        }

        if (shown & FIELD_TIME) {
            if (has_any)
                printf(" ");
            printf("TIME:[%s]", e->time);
            has_any = 1;
        }
        if (shown & FIELD_NAME) {
            if (has_any)
                printf(" ");
            printf("NAME:[%s]", e->name);
            has_any = 1;
        }
        if (shown & FIELD_PRIORITY) {
            if (has_any)
                printf(" ");
            printf("PRIORITY:[%d]", e->priority);
            has_any = 1;
        }
        if (shown & FIELD_STATUS) {
            if (has_any)
                printf(" ");
            printf("STATUS:[%s]", e->status);
            has_any = 1;
        }
        if (shown & FIELD_TAGS) {
            if (has_any)
                printf(" ");
            printf("TAGS:[");
            for (int j = 0; j < dalen(e->tags); j++) {
                if (strcmp(e->tags[j], "") == 0)
                    continue;
                printf("%s", e->tags[j]);
                if (j < dalen(e->tags) - 1 && strcmp(e->tags[j + 1], "") != 0)
                    printf(",");
            }
            printf("]");
        }
        if (has_any)
            printf("\n");
    }
}

static Error templates_dir_init() {
    char *main_dir = find_dir_up(g_config.main_dir_name);
    if (!main_dir) {
        fprintf(stderr, "Error: Entries directory not found\n");
        return ERR_FAILURE;
    }

    char templates_dir[PATH_MAX - 15];
    snprintf(templates_dir, sizeof(templates_dir), "%s/%s", main_dir,
             g_config.templates_dir_name);

    if (!file_exists(templates_dir)) {
        if (mkdir(templates_dir, 0755) != 0) {
            free(main_dir);
            fprintf(stderr, "Error: Failed to create templates directory: %s\n",
                    templates_dir);
            return ERR_FAILURE;
        }
        printf("Created templates directory: %s\n", templates_dir);
    } else {
        printf("Templates directory already exists: %s\n", templates_dir);
    }

    char default_template[PATH_MAX];
    snprintf(default_template, sizeof(default_template), "%s/default.md",
             templates_dir);

    if (!file_exists(default_template)) {
        FILE *f = fopen(default_template, "w");
        if (f) {
            fprintf(f, "- NAME:\n");
            fprintf(f, "- PRIORITY:\n");
            fprintf(f, "- TAGS:\n");
            fprintf(f, "- STATUS:\n");
            fclose(f);
            printf("Created default template: %s\n", default_template);
        } else {
            free(main_dir);
            fprintf(stderr, "Error: Failed to create default template: %s\n",
                    default_template);
            return ERR_FAILURE;
        }
    } else {
        printf("Default template already exists: %s\n", default_template);
    }

    free(main_dir);
    return ERR_SUCCESS;
}

static Error entries_dir_init() {
    char *cwd = get_cwd();
    char entries_dir[PATH_MAX];
    snprintf(entries_dir, sizeof(entries_dir), "%s/%s", cwd,
             g_config.main_dir_name);

    if (file_exists(entries_dir)) {
        printf("Entries directory already exists: %s\n", entries_dir);
    } else {
        if (mkdir(entries_dir, 0755) == 0) {
            printf("Created entries directory: %s\n", entries_dir);
        } else {
            free(cwd);
            fprintf(stderr, "Error: Failed to create entries directory: %s\n",
                    entries_dir);
            return ERR_FAILURE;
        }
    }

    free(cwd);
    return ERR_SUCCESS;
}

static Error entry_create(const char *entry_path, const char *template_name) {
    char entry_md[PATH_MAX];
    snprintf(entry_md, sizeof(entry_md), "%s/%s", entry_path,
             g_config.entry_file_name);

    if (template_name) {
        char template_file[PATH_MAX];
        char *main_dir = find_dir_up(g_config.main_dir_name);
        if (!main_dir) {
            fprintf(stderr, "Error: Entries directory not found\n");
            return ERR_FAILURE;
        }

        snprintf(template_file, sizeof(template_file), "%s/%s/%s.md", main_dir,
                 g_config.templates_dir_name, template_name);
        free(main_dir);

        if (file_exists(template_file)) {
            FILE *src = fopen(template_file, "r");
            if (!src) {
                fprintf(stderr, "Error: Failed to open template: %s\n",
                        template_file);
                return ERR_FAILURE;
            }

            FILE *dst = fopen(entry_md, "w");
            if (!dst) {
                fclose(src);
                fprintf(stderr, "Error: Failed to create %s: %s\n",
                        g_config.entry_file_name, entry_md);
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
            fprintf(stderr, "Error: Template '%s' was not found\n",
                    template_name);
            if (strcmp(template_name, "default") == 0) {
                fprintf(stderr, "Hint: Create default template with '%s -i'\n",
                        argv0);
            }
            return ERR_FAILURE;
        }
    }

    FILE *f = fopen(entry_md, "w");
    if (!f) {
        fprintf(stderr, "Error: Failed to create %s in: %s\n",
                g_config.entry_file_name, entry_path);
        return ERR_FAILURE;
    }
    fclose(f);
    return ERR_SUCCESS;
}

static Error entry_create_dir_and_md(const char *main_dir, OutputFormat fmt) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);

    char entry_path[PATH_MAX - 10];
    snprintf(entry_path, sizeof(entry_path), "%s/%s", main_dir, dir_name);

    if (file_exists(entry_path)) {
        fprintf(stderr, "Error: Entry directory already exists\n");
        return ERR_FAILURE;
    }
    if (mkdir(entry_path, 0755) != 0) {
        fprintf(stderr, "Error: Failed to create entry directory\n");
        return ERR_FAILURE;
    }

    if (entry_create(entry_path, g_config.template_name) != ERR_SUCCESS) {
        return ERR_FAILURE;
    }

    Entry entry = {
        .path = entry_path,
        .name = "",
        .status = "",
        .time = dir_name,
    };

    entry_print(&entry, fmt);
    return ERR_SUCCESS;
}

static Entry *entry_parse(const char *entry_dir, const char *entry_time) {
    char entry_file[PATH_MAX];
    snprintf(entry_file, sizeof(entry_file), "%s/%s", entry_dir,
             g_config.entry_file_name);

    FILE *f = fopen(entry_file, "r");
    if (!f)
        return NULL;

    Entry *entry = calloc(1, sizeof(Entry));
    entry->priority = 0;
    entry->path = strdup(entry_dir);
    entry->status = strdup("");
    entry->name = strdup("");
    entry->time = strdup(entry_time);

    char *line = NULL;
    size_t line_len = 0;
    ssize_t read;
    int lines_processed = 0;

    while (lines_processed < g_config.max_header_lines &&
           (read = getline(&line, &line_len, f)) != -1) {
        if (read > 0 && line[read - 1] == '\n')
            line[read - 1] = '\0';
        lines_processed++;

        char *value;

        if ((value = regex_extract_first_group(line, &g_name_regex)) != NULL) {
            free(entry->name);
            entry->name = strdup(trim(value));
            free(value);
        } else if ((value = regex_extract_first_group(
                        line, &g_priority_regex)) != NULL) {
            entry->priority = atoi(value);
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
                        dapush(entry->tags, strdup(clean));
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            free(value);
        } else if ((value = regex_extract_first_group(line, &g_status_regex)) !=
                   NULL) {
            free(entry->status);
            entry->status = strdup(trim(value));
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
        snprintf(entry_dir, sizeof(entry_dir), "%s/%s", main_dir,
                 dirent->d_name);

        char entry_file[PATH_MAX];
        snprintf(entry_file, sizeof(entry_file), "%s/%s", entry_dir,
                 g_config.entry_file_name);

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
            return entry_matches_condition(entry, node->binary.left) &&
                   entry_matches_condition(entry, node->binary.right);
        case OP_OR:
            return entry_matches_condition(entry, node->binary.left) ||
                   entry_matches_condition(entry, node->binary.right);
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
            case CMP_TILDE: // fallthrough
            case CMP_EQ:
                return entry_val == cond_val;
            case CMP_NTILDE: // fallthrough
            case CMP_NE:
                return entry_val != cond_val;
            default:
                return 0;
            }
        } // case CMP_PRIORITY

        case CMP_TAG: {
            char *cond_tag = node->comparison.value.str_value;
            for (int i = 0; i < dalen(entry->tags); i++) {
                if (node->comparison.cmp == CMP_EQ) {
                    if (strcmp(entry->tags[i], cond_tag) == 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_NE) {
                    if (strcmp(entry->tags[i], cond_tag) == 0)
                        return 0;
                } else if (node->comparison.cmp == CMP_TILDE) {
                    if (strstr(entry->tags[i], cond_tag) != NULL)
                        return 1;
                } else if (node->comparison.cmp == CMP_NTILDE) {
                    if (strstr(entry->tags[i], cond_tag) != NULL)
                        return 0;
                } else if (node->comparison.cmp == CMP_GT) {
                    if (strcmp(entry->tags[i], cond_tag) > 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_LT) {
                    if (strcmp(entry->tags[i], cond_tag) < 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_GE) {
                    if (strcmp(entry->tags[i], cond_tag) >= 0)
                        return 1;
                } else if (node->comparison.cmp == CMP_LE) {
                    if (strcmp(entry->tags[i], cond_tag) <= 0)
                        return 1;
                }
            }
            return (node->comparison.cmp == CMP_NE ||
                    node->comparison.cmp == CMP_NTILDE);
        } // case CMP_TAG

        case CMP_STATUS: {
            char *cond_status = node->comparison.value.str_value;
            if (!entry->status)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->status, cond_status) == 0;
            case CMP_NE:
                return strcmp(entry->status, cond_status) != 0;
            case CMP_TILDE:
                return strstr(entry->status, cond_status) != NULL;
            case CMP_NTILDE:
                return strstr(entry->status, cond_status) == NULL;
            case CMP_GT:
                return strcmp(entry->status, cond_status) > 0;
            case CMP_LT:
                return strcmp(entry->status, cond_status) < 0;
            case CMP_GE:
                return strcmp(entry->status, cond_status) >= 0;
            case CMP_LE:
                return strcmp(entry->status, cond_status) <= 0;
            default:
                return 0;
            }
        } // case CMP_STATUS

        case CMP_TIME: {
            char *cond_time = node->comparison.value.str_value;
            if (!entry->time)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->time, cond_time) == 0;
            case CMP_NE:
                return strcmp(entry->time, cond_time) != 0;
            case CMP_TILDE:
                return strstr(entry->time, cond_time) != NULL;
            case CMP_NTILDE:
                return strstr(entry->time, cond_time) == NULL;
            case CMP_GT:
                return strcmp(entry->time, cond_time) > 0;
            case CMP_LT:
                return strcmp(entry->time, cond_time) < 0;
            case CMP_GE:
                return strcmp(entry->time, cond_time) >= 0;
            case CMP_LE:
                return strcmp(entry->time, cond_time) <= 0;
            default:
                return 0;
            }
        } // case CMP_TIME

        case CMP_NAME: {
            char *cond_name = node->comparison.value.str_value;
            if (!entry->name)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->name, cond_name) == 0;
            case CMP_NE:
                return strcmp(entry->name, cond_name) != 0;
            case CMP_TILDE:
                return strstr(entry->name, cond_name) != NULL;
            case CMP_NTILDE:
                return strstr(entry->name, cond_name) == NULL;
            case CMP_GT:
                return strcmp(entry->name, cond_name) > 0;
            case CMP_LT:
                return strcmp(entry->name, cond_name) < 0;
            case CMP_GE:
                return strcmp(entry->name, cond_name) >= 0;
            case CMP_LE:
                return strcmp(entry->name, cond_name) <= 0;
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

static Entry **entries_filter(Entry **entries, ASTNode *filter) {
    if (!entries || dalen(entries) == 0 || !filter) {
        return entries;
    }

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
    OutputFormat fmt = (OutputFormat)(intptr_t)ctx;
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

static Error entries_process_with_filter(const char *query,
                                         entry_operation_fn op, void *ctx) {
    // compile filter
    ASTNode *filter = parse(query);
    if (!filter) {
        fprintf(stderr, "Error: Failed to parse query: %s\n", query);
        return ERR_FAILURE;
    }

    // find main dir
    char *main_dir = find_dir_up(g_config.main_dir_name);
    if (!main_dir) {
        ast_free(filter);
        fprintf(stderr, "Error: Entries directory '%s' not found\n",
                g_config.main_dir_name);
        return ERR_FAILURE;
    }

    // find entries
    Entry **entries = entries_get_all(main_dir);
    entries = entries_filter(entries, filter);

    // execute operation
    op(entries, ctx);

    // cleanup
    entries_free(entries);
    free(main_dir);
    ast_free(filter);

    return ERR_SUCCESS;
}

// ================ ENTRYPOINT

static void usage() {
    fprintf(stdout,
            "usage: %s [-h] [-i] [-n] [-t template] [-D dir] [-f format] [-p "
            "query] [-r query] [-NTPSAH]\n"
            "  -h           show this help\n"
            "  -i           initialize main directory in current location\n"
            "  -t template  use template file from %s/%s/<template>.md\n"
            "  -n           create new entry\n"
            "  -D dir       use custom main directory name instead of '%s'\n"
            "  -p query     print entries using query (e.g. 'priority > 5')\n"
            "  -r query     remove entries matching query\n"
            "  -f format    output format for -p:\n"
            "                 unix: path:1:1: STATUS:[...] NAME:[...] ... "
            "(default)\n"
            "                 path: absolute paths only, one per line\n"
            "                 jsonl: newline-delimited JSON\n"
            "  -N           hide name field in output\n"
            "  -T           hide time field in output\n"
            "  -P           hide priority field in output\n"
            "  -S           hide status field in output\n"
            "  -A           hide tags field in output\n"
            "  -H           hide path field in output\n",
            argv0, g_config.main_dir_name, g_config.templates_dir_name,
            g_config.main_dir_name);
}

int main(int argc, char **argv) {
    if (init_regexes() != ERR_SUCCESS) {
        return ERR_FAILURE;
    }
    atexit(free_regexes);

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
    case 'D': {
        const char *dir_str = ARGF();
        if (!dir_str) {
            fprintf(stderr, "Error: -D requires a directory name argument\n");
            return ERR_FAILURE;
        }
        g_config.main_dir_name = dir_str;
        break;
    }
    case 'n': {
        do_new = 1;
        break;
    }
    case 't': {
        const char *template_name = ARGF();
        if (!template_name) {
            fprintf(stderr, "Error: -t requires a template name argument\n");
            return ERR_FAILURE;
        }
        g_config.template_name = template_name;
        break;
    }
    case 'f': {
        const char *fmt_str = ARGF();
        if (!fmt_str) {
            fprintf(stderr, "Error: -f requires a format argument\n");
            return ERR_FAILURE;
        }
        if (strcmp(fmt_str, "path") == 0) {
            fmt = FMT_ONLY_PATH;
        } else if (strcmp(fmt_str, "unix") == 0) {
            fmt = FMT_UNIX;
        } else if (strcmp(fmt_str, "jsonl") == 0) {
            fmt = FMT_JSONL;
        } else {
            fprintf(stderr, "Error: Unknown format '%s'\n", fmt_str);
            return ERR_FAILURE;
        }
        break;
    }
    case 'p': {
        query = ARGF();
        if (!query) {
            fprintf(stderr, "Error: -p requires a query argument\n");
            return ERR_FAILURE;
        }
        do_print = 1;
        break;
    }
    case 'r': {
        query = ARGF();
        if (!query) {
            fprintf(stderr, "Error: -r requires a query argument\n");
            return ERR_FAILURE;
        }
        do_remove = 1;
        break;
    }
    // hide field
    case 'N':
        g_config.hide_fields |= FIELD_NAME;
        break;
    case 'T':
        g_config.hide_fields |= FIELD_TIME;
        break;
    case 'P':
        g_config.hide_fields |= FIELD_PRIORITY;
        break;
    case 'S':
        g_config.hide_fields |= FIELD_STATUS;
        break;
    case 'A':
        g_config.hide_fields |= FIELD_TAGS;
        break;
    case 'H':
        g_config.hide_fields |= FIELD_PATH;
        break;
    default: {
        fprintf(stderr, "Error: Unknown flag '%c'\n", ARGC());
        return ERR_FAILURE;
    }
    }
    ARGEND;

    if (do_help) {
        usage();
        return ERR_SUCCESS;
    } else if (do_init) {
        Error err = entries_dir_init();
        if (err != ERR_SUCCESS)
            return err;
        err = templates_dir_init();
        if (err != ERR_SUCCESS)
            return err;
        return ERR_SUCCESS;
    } else if (do_new) {
        char *main_dir = find_dir_up(g_config.main_dir_name);
        if (!main_dir) {
            fprintf(stderr, "Error: Entries directory not found\n");
            return ERR_FAILURE;
        }
        Error ret = entry_create_dir_and_md(main_dir, fmt);
        free(main_dir);
        return ret;
    } else if (do_print) {
        return entries_process_with_filter(query, entry_op_print,
                                           (void *)(intptr_t)fmt);
    } else if (do_remove) {
        return entries_process_with_filter(query, entry_op_delete, NULL);
    }

    usage();
    return ERR_SUCCESS;
}
