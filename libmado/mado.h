#ifndef MADO_H
#define MADO_H

#include <stdint.h>

typedef enum {
    MADO_FMT_UNIX,
    MADO_FMT_ONLY_PATH,
    MADO_FMT_JSONL,
} Mado_Output_Format;

typedef enum {
    MADO_FIELD_NONE = 0,
    MADO_FIELD_NAME = 1 << 0,
    MADO_FIELD_TIME = 1 << 1,
    MADO_FIELD_DEADLINE = 1 << 2,
    MADO_FIELD_PRIORITY = 1 << 3,
    MADO_FIELD_STATUS = 1 << 4,
    MADO_FIELD_TAGS = 1 << 5,
    MADO_FIELD_PATH = 1 << 6,
    MADO_FIELD_ALL = MADO_FIELD_NAME | MADO_FIELD_TIME | MADO_FIELD_DEADLINE |
                     MADO_FIELD_PRIORITY | MADO_FIELD_STATUS | MADO_FIELD_TAGS |
                     MADO_FIELD_PATH
} Mado_Entry_Field;

typedef struct Mado_Entry {
    char *path;
    char *name;
    uint16_t priority;
    char **tags;
    char *status;
    char *time;
    char *deadline;
} Mado_Entry;

typedef void (*Mado_Entry_Operation_Fn)(Mado_Entry **entries, void *ctx);

typedef struct Mado_Config {
    const char *main_dir_name;
    const char *templates_dir_name;
    const char *entry_file_name;
    const char *template_name;
    int max_header_lines;
    int max_header_line_len;
    Mado_Entry_Field hide_fields;
} Mado_Config;

// Lifecycle
int mado_init(Mado_Config *cfg);
void mado_deinit();

// Entries
Mado_Entry *mado_entry_parse(const Mado_Config *cfg, const char *entry_dir);
void mado_entry_print(const Mado_Config *cfg, Mado_Entry *e, Mado_Output_Format fmt);
void mado_entry_free(Mado_Entry *entry);
Mado_Entry **mado_entries_get_all(const Mado_Config *cfg, const char *main_dir);
void mado_entries_free(Mado_Entry **entries);

// Filter & process
int mado_entries_process_with_filter(const Mado_Config *cfg, const char *query, Mado_Entry_Operation_Fn op, void *ctx);
void mado_entry_op_print(Mado_Entry **entries, void *ctx);
void mado_entry_op_delete(Mado_Entry **entries, void *ctx);

// Repo management
int mado_templates_dir_init(const Mado_Config *cfg);
int mado_entries_dir_init(const Mado_Config *cfg, int force);
int mado_entry_create_dir_and_md(const Mado_Config *cfg, const char *main_dir, Mado_Output_Format fmt);
int mado_print_repo_info(const Mado_Config *cfg);

// Utils
int mado_parse_format(const char *format_str, Mado_Output_Format *fmt);

#endif
