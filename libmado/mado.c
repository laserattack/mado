#include <dirent.h>
#include <limits.h>
#include <regex.h>
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
#include "mado.h"

#define UNUSED(x) (void)(x)

// ================ CONFIG

void mado_init_config(Mado_Config *cfg) {
    cfg->main_dir_name = "MADO";
    cfg->templates_dir_name = ".templates";
    cfg->entry_file_name = "MAIN";
    cfg->template_name = "task";
    cfg->max_header_lines = 30;
    cfg->max_header_line_len = 1024;
    cfg->hide_fields = MADO_FIELD_NONE;
}

// ================ REGEX

static regex_t g_entry_dir_regex;
static regex_t g_name_regex;
static regex_t g_priority_regex;
static regex_t g_tags_regex;
static regex_t g_status_regex;
static regex_t g_deadline_regex;
static int g_regex_initialized = 0;

int mado_init_regexes(void) {
    if (g_regex_initialized)
        return 0;
    if (regcomp(&g_entry_dir_regex, "^[0-9]{8}T[0-9]{6}$", REG_EXTENDED) != 0)
        return -1;
    if (regcomp(&g_name_regex, "^- NAME:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        return -1;
    if (regcomp(&g_priority_regex, "^- PRIORITY:[[:space:]]*([0-9]{1,3})$", REG_EXTENDED) != 0)
        return -1;
    if (regcomp(&g_tags_regex, "^- TAGS:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        return -1;
    if (regcomp(&g_status_regex, "^- STATUS:[[:space:]]*(.*)$", REG_EXTENDED) != 0)
        return -1;
    if (regcomp(&g_deadline_regex, "^- DEADLINE:[[:space:]]*([0-9]{4}|[0-9]{6}|[0-9]{8}(T([0-9]{2}|[0-9]{4}|[0-9]{6})?)?)[[:space:]]*$", REG_EXTENDED) != 0)
        return -1;
    g_regex_initialized = 1;
    return 0;
}

void mado_free_regexes(void) {
    if (!g_regex_initialized)
        return;
    regfree(&g_entry_dir_regex);
    regfree(&g_name_regex);
    regfree(&g_priority_regex);
    regfree(&g_tags_regex);
    regfree(&g_status_regex);
    regfree(&g_deadline_regex);
}

// ================ PRINT

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

void mado_entry_print(const Mado_Config *cfg, Mado_Entry *e, Mado_Output_Format fmt) {
    Mado_Entry_Field hidden = cfg->hide_fields;
    Mado_Entry_Field shown = MADO_FIELD_ALL & ~hidden;

    if (fmt == MADO_FMT_ONLY_PATH) {
        printf("%s/%s.md\n", e->path, cfg->entry_file_name);
        return;
    }

    if (fmt == MADO_FMT_JSONL) {
        printf("{");
        int has_any = 0;
        if (shown & MADO_FIELD_TIME) {
            if (has_any)
                printf(",");
            printf("\"time\":");
            print_json_string(e->time);
            has_any = 1;
        }
        if (shown & MADO_FIELD_NAME) {
            if (has_any)
                printf(",");
            printf("\"name\":");
            print_json_string(e->name);
            has_any = 1;
        }
        if (shown & MADO_FIELD_PRIORITY) {
            if (has_any)
                printf(",");
            printf("\"priority\":%d", e->priority);
            has_any = 1;
        }
        if (shown & MADO_FIELD_DEADLINE) {
            if (has_any)
                printf(",");
            printf("\"deadline\":");
            print_json_string(e->deadline);
            has_any = 1;
        }
        if (shown & MADO_FIELD_STATUS) {
            if (has_any)
                printf(",");
            printf("\"status\":");
            print_json_string(e->status);
            has_any = 1;
        }
        if (shown & MADO_FIELD_TAGS) {
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
        if (shown & MADO_FIELD_PATH) {
            if (has_any)
                printf(",");
            printf("\"path\":\"%s/%s.md\"", e->path, cfg->entry_file_name);
            has_any = 1;
        }
        printf("}\n");
        return;
    }

    if (fmt == MADO_FMT_UNIX) {
        printf("%s/%s.md:1:1:", e->path, cfg->entry_file_name);
        if (shown & MADO_FIELD_TIME)
            printf(" TIME:[%s]", e->time);
        if (shown & MADO_FIELD_NAME)
            printf(" NAME:[%s]", e->name);
        if (shown & MADO_FIELD_PRIORITY)
            printf(" PRIORITY:[%d]", e->priority);
        if (shown & MADO_FIELD_DEADLINE)
            printf(" DEADLINE:[%s]", e->deadline);
        if (shown & MADO_FIELD_STATUS)
            printf(" STATUS:[%s]", e->status);
        if (shown & MADO_FIELD_TAGS) {
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

// ================ INIT

int mado_templates_dir_init(const Mado_Config *cfg) {
    char *main_dir = find_dir_up(cfg->main_dir_name);
    if (!main_dir) {
        fprintf(stderr, "Error: entries directory '%s' not found\n", cfg->main_dir_name);
        return -1;
    }

    char templates_dir[PATH_MAX - 15];
    snprintf(templates_dir, sizeof(templates_dir), "%s/%s", main_dir, cfg->templates_dir_name);

    if (!file_exists(templates_dir)) {
        if (mkdir(templates_dir, 0755) != 0) {
            free(main_dir);
            fprintf(stderr, "Error: failed to create templates directory: %s\n", templates_dir);
            return -1;
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
            return -1;
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
            return -1;
        }
    } else {
        printf("Note template already exists: %s\n", note_template);
    }

    free(main_dir);
    return 0;
}

int mado_entries_dir_init(const Mado_Config *cfg, int force) {
    char *cwd = get_cwd();
    char entries_dir[PATH_MAX];
    snprintf(entries_dir, sizeof(entries_dir), "%s/%s", cwd, cfg->main_dir_name);

    if (!force) {
        char *entries_dir_existing = find_dir_up(cfg->main_dir_name);
        if (entries_dir_existing) {
            printf("Entries directory already exists: %s\n", entries_dir_existing);
            if (strcmp(entries_dir_existing, entries_dir))
                printf("Hint: use -F to force initialization in current directory\n");
            free(entries_dir_existing);
            free(cwd);
            return 0;
        }
    }

    if (file_exists(entries_dir)) {
        printf("Entries directory already exists: %s\n", entries_dir);
        if (force)
            printf("Hint: -F has no effect because '%s' already exists in this location\n", cfg->main_dir_name);
    } else {
        if (mkdir(entries_dir, 0755) == 0) {
            printf("Created entries directory: %s\n", entries_dir);
        } else {
            free(cwd);
            fprintf(stderr, "Error: failed to create entries directory: %s\n", entries_dir);
            return -1;
        }
    }

    free(cwd);
    return 0;
}

int mado_entry_create(const Mado_Config *cfg, const char *entry_path, const char *template_name) {
    char entry_md[PATH_MAX];
    snprintf(entry_md, sizeof(entry_md), "%s/%s.md", entry_path, cfg->entry_file_name);

    if (template_name) {
        char template_file[PATH_MAX];
        char *main_dir = find_dir_up(cfg->main_dir_name);
        if (!main_dir) {
            fprintf(stderr, "Error: entries directory '%s' not found\n", cfg->main_dir_name);
            return -1;
        }

        snprintf(template_file, sizeof(template_file), "%s/%s/%s.md", main_dir, cfg->templates_dir_name, template_name);
        free(main_dir);

        if (file_exists(template_file)) {
            FILE *src = fopen(template_file, "r");
            if (!src) {
                fprintf(stderr, "Error: failed to open template: %s\n", template_file);
                return -1;
            }
            FILE *dst = fopen(entry_md, "w");
            if (!dst) {
                fclose(src);
                fprintf(stderr, "Error: failed to create %s.md: %s\n", cfg->entry_file_name, entry_md);
                return -1;
            }
            char buffer[4096];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
                fwrite(buffer, 1, n, dst);
            fclose(src);
            fclose(dst);
            return 0;
        } else {
            fprintf(stderr, "Error: template '%s' was not found\n", template_name);
            return -1;
        }
    }

    FILE *f = fopen(entry_md, "w");
    if (!f) {
        fprintf(stderr, "Error: failed to create %s.md in: %s\n", cfg->entry_file_name, entry_path);
        return -1;
    }
    fclose(f);
    return 0;
}

int mado_entry_create_dir_and_md(const Mado_Config *cfg, const char *main_dir, Mado_Output_Format fmt) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);
    char entry_path[PATH_MAX - 10];
    snprintf(entry_path, sizeof(entry_path), "%s/%s", main_dir, dir_name);

    if (file_exists(entry_path)) {
        fprintf(stderr, "Error: entry directory already exists\n");
        return -1;
    }
    if (mkdir(entry_path, 0755) != 0) {
        fprintf(stderr, "Error: failed to create entry directory\n");
        return -1;
    }
    if (mado_entry_create(cfg, entry_path, cfg->template_name) != 0)
        return -1;

    Mado_Entry entry = {.path = entry_path, .name = "", .status = "", .time = dir_name, .deadline = ""};
    mado_entry_print(cfg, &entry, fmt);
    return 0;
}

// ================ PARSE

Mado_Entry *mado_entry_parse(const Mado_Config *cfg, const char *entry_dir, const char *entry_time) {
    char entry_file[PATH_MAX];
    snprintf(entry_file, sizeof(entry_file), "%s/%s.md", entry_dir, cfg->entry_file_name);

    FILE *f = fopen(entry_file, "r");
    if (!f)
        return NULL;

    Mado_Entry *entry = calloc(1, sizeof(Mado_Entry));
    entry->priority = 0;
    entry->path = strdup(entry_dir);
    entry->status = strdup("");
    entry->name = strdup("");
    entry->time = strdup(entry_time);
    entry->deadline = strdup("99990000T000000");

    char *line = malloc(cfg->max_header_line_len);
    int lines_processed = 0;

    while (lines_processed < cfg->max_header_lines && fgets(line, cfg->max_header_line_len, f)) {
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

Mado_Entry **mado_entries_get_all(const Mado_Config *cfg, const char *main_dir) {
    DIR *dir = opendir(main_dir);
    if (!dir)
        return NULL;

    Mado_Entry **entries = NULL;
    struct dirent *dirent;

    while ((dirent = readdir(dir)) != NULL) {
        if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0 ||
            !regex_match(dirent->d_name, &g_entry_dir_regex))
            continue;

        char entry_dir[PATH_MAX - 10];
        snprintf(entry_dir, sizeof(entry_dir), "%s/%s", main_dir, dirent->d_name);
        char entry_file[PATH_MAX];
        snprintf(entry_file, sizeof(entry_file), "%s/%s.md", entry_dir, cfg->entry_file_name);
        if (!file_exists(entry_file))
            continue;

        Mado_Entry *entry = mado_entry_parse(cfg, entry_dir, dirent->d_name);
        if (entry)
            dapush(entries, entry);
    }

    closedir(dir);
    return entries;
}

// ================ FREE

void mado_entry_free(Mado_Entry *entry) {
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

void mado_entries_free(Mado_Entry **entries) {
    if (!entries)
        return;
    for (int i = 0; i < dalen(entries); i++)
        mado_entry_free(entries[i]);
    dafree(entries);
}

// ================ FILTER

int mado_entry_matches_condition(Mado_Entry *entry, ASTNode *node) {
    if (!node)
        return 1;
    switch (node->type) {
    case NODE_ALL:
        return 1;
    case NODE_BINARY_OP:
        switch (node->binary.op) {
        case OP_AND:
            return mado_entry_matches_condition(entry, node->binary.left) && mado_entry_matches_condition(entry, node->binary.right);
        case OP_OR:
            return mado_entry_matches_condition(entry, node->binary.left) || mado_entry_matches_condition(entry, node->binary.right);
        default:
            return 0;
        }
    case NODE_UNARY_OP:
        if (node->unary.op == OP_NOT)
            return !mado_entry_matches_condition(entry, node->unary.expr);
        return 0;
    case NODE_COMPARISON: {
        switch (node->comparison.field) {
        case CMP_PRIORITY: {
            int ev = entry->priority, cv = node->comparison.value.int_value;
            switch (node->comparison.cmp) {
            case CMP_GT:
                return ev > cv;
            case CMP_LT:
                return ev < cv;
            case CMP_GE:
                return ev >= cv;
            case CMP_LE:
                return ev <= cv;
            case CMP_TILDE:
            case CMP_EQ:
                return ev == cv;
            case CMP_NTILDE:
            case CMP_NE:
                return ev != cv;
            default:
                return 0;
            }
        }
        case CMP_TAG: {
            char *ct = node->comparison.value.str_value;
            for (int i = 0; i < dalen(entry->tags); i++) {
                switch (node->comparison.cmp) {
                case CMP_EQ:
                    if (strcmp(entry->tags[i], ct) == 0)
                        return 1;
                    break;
                case CMP_NE:
                    if (strcmp(entry->tags[i], ct) == 0)
                        return 0;
                    break;
                case CMP_TILDE:
                    if (strstr(entry->tags[i], ct))
                        return 1;
                    break;
                case CMP_NTILDE:
                    if (strstr(entry->tags[i], ct))
                        return 0;
                    break;
                case CMP_GT:
                    if (strcmp(entry->tags[i], ct) > 0)
                        return 1;
                    break;
                case CMP_LT:
                    if (strcmp(entry->tags[i], ct) < 0)
                        return 1;
                    break;
                case CMP_GE:
                    if (strcmp(entry->tags[i], ct) >= 0)
                        return 1;
                    break;
                case CMP_LE:
                    if (strcmp(entry->tags[i], ct) <= 0)
                        return 1;
                    break;
                default:
                    break;
                }
            }
            return (node->comparison.cmp == CMP_NE || node->comparison.cmp == CMP_NTILDE);
        }
        case CMP_STATUS: {
            char *c = node->comparison.value.str_value;
            if (!entry->status)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->status, c) == 0;
            case CMP_NE:
                return strcmp(entry->status, c) != 0;
            case CMP_TILDE:
                return strstr(entry->status, c) != NULL;
            case CMP_NTILDE:
                return strstr(entry->status, c) == NULL;
            case CMP_GT:
                return strcmp(entry->status, c) > 0;
            case CMP_LT:
                return strcmp(entry->status, c) < 0;
            case CMP_GE:
                return strcmp(entry->status, c) >= 0;
            case CMP_LE:
                return strcmp(entry->status, c) <= 0;
            default:
                return 0;
            }
        }
        case CMP_DEADLINE: {
            char *c = node->comparison.value.str_value;
            if (!entry->deadline)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->deadline, c) == 0;
            case CMP_NE:
                return strcmp(entry->deadline, c) != 0;
            case CMP_TILDE:
                return strstr(entry->deadline, c) != NULL;
            case CMP_NTILDE:
                return strstr(entry->deadline, c) == NULL;
            case CMP_GT:
                return strcmp(entry->deadline, c) > 0;
            case CMP_LT:
                return strcmp(entry->deadline, c) < 0;
            case CMP_GE:
                return strcmp(entry->deadline, c) >= 0;
            case CMP_LE:
                return strcmp(entry->deadline, c) <= 0;
            default:
                return 0;
            }
        }
        case CMP_TIME: {
            char *c = node->comparison.value.str_value;
            if (!entry->time)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->time, c) == 0;
            case CMP_NE:
                return strcmp(entry->time, c) != 0;
            case CMP_TILDE:
                return strstr(entry->time, c) != NULL;
            case CMP_NTILDE:
                return strstr(entry->time, c) == NULL;
            case CMP_GT:
                return strcmp(entry->time, c) > 0;
            case CMP_LT:
                return strcmp(entry->time, c) < 0;
            case CMP_GE:
                return strcmp(entry->time, c) >= 0;
            case CMP_LE:
                return strcmp(entry->time, c) <= 0;
            default:
                return 0;
            }
        }
        case CMP_NAME: {
            char *c = node->comparison.value.str_value;
            if (!entry->name)
                return 0;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return strcmp(entry->name, c) == 0;
            case CMP_NE:
                return strcmp(entry->name, c) != 0;
            case CMP_TILDE:
                return strstr(entry->name, c) != NULL;
            case CMP_NTILDE:
                return strstr(entry->name, c) == NULL;
            case CMP_GT:
                return strcmp(entry->name, c) > 0;
            case CMP_LT:
                return strcmp(entry->name, c) < 0;
            case CMP_GE:
                return strcmp(entry->name, c) >= 0;
            case CMP_LE:
                return strcmp(entry->name, c) <= 0;
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

Mado_Entry **mado_entries_filter(Mado_Entry **entries, ASTNode *filter) {
    if (!entries || dalen(entries) == 0 || !filter)
        return entries;
    Mado_Entry **filtered = NULL;
    for (int i = 0; i < dalen(entries); i++) {
        Mado_Entry *e = entries[i];
        if (mado_entry_matches_condition(e, filter))
            dapush(filtered, e);
        else
            mado_entry_free(e);
    }
    dafree(entries);
    return filtered;
}

// ================ OPERATIONS

void mado_entry_op_print(Mado_Entry **entries, void *ctx) {
    struct {
        const Mado_Config *cfg;
        Mado_Output_Format fmt;
    } *p = ctx;
    if (!entries)
        return;
    for (int i = 0; i < dalen(entries); i++)
        mado_entry_print(p->cfg, entries[i], p->fmt);
}

void mado_entry_op_delete(Mado_Entry **entries, void *ctx) {
    UNUSED(ctx);
    if (!entries)
        return;
    for (int i = 0; i < dalen(entries); i++) {
        Mado_Entry *e = entries[i];
        rmrf(e->path);
        printf("Removed: %s\n", e->path);
    }
}

int mado_entries_process_with_filter(const Mado_Config *cfg, const char *query, Mado_Entry_Operation_Fn op, void *ctx) {
    ASTNode *filter = NULL;
    if (query) {
        filter = parse(query);
        if (!filter) {
            fprintf(stderr, "Error: failed to parse query\n");
            return -1;
        }
    }

    char *main_dir = find_dir_up(cfg->main_dir_name);
    if (!main_dir) {
        ast_free(filter);
        fprintf(stderr, "Error: entries directory '%s' not found\n", cfg->main_dir_name);
        return -1;
    }

    Mado_Entry **entries = mado_entries_get_all(cfg, main_dir);
    entries = mado_entries_filter(entries, filter);
    op(entries, ctx);
    mado_entries_free(entries);
    free(main_dir);
    ast_free(filter);
    return 0;
}

int mado_print_repo_info(const Mado_Config *cfg) {
    char *main_dir = find_dir_up(cfg->main_dir_name);
    if (!main_dir) {
        printf("No mado repository here\n");
        return 0;
    }
    Mado_Entry **entries = mado_entries_get_all(cfg, main_dir);
    int count = entries ? dalen(entries) : 0;
    printf("Main directory: %s\n", main_dir);
    printf("Entries count:  %d\n", count);
    mado_entries_free(entries);
    free(main_dir);
    return 0;
}

int mado_parse_format(const char *format_str, Mado_Output_Format *fmt) {
    if (strcmp(format_str, "path") == 0)
        *fmt = MADO_FMT_ONLY_PATH;
    else if (strcmp(format_str, "unix") == 0)
        *fmt = MADO_FMT_UNIX;
    else if (strcmp(format_str, "jsonl") == 0)
        *fmt = MADO_FMT_JSONL;
    else
        return 0;
    return 1;
}
