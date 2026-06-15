#ifndef MADO_HPP
#define MADO_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "ast.h"
}

// ================ ENUMS

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

// ================ CONFIG

struct Mado_Config {
    std::string main_dir_name;
    std::string templates_dir_name;
    std::string entry_file_name;
    std::string template_name;
    int max_header_lines;
    Mado_Entry_Field hide_fields;
};

// ================ ENTRY

class Mado_Entry {
  public:
    std::string path;
    std::string name;
    uint16_t priority = 0;
    std::vector<std::string> tags;
    std::string status;
    std::string time;
    std::string deadline;

    static std::unique_ptr<Mado_Entry> parse(const Mado_Config *cfg, const char *entry_dir);
    void print(const Mado_Config *cfg, Mado_Output_Format fmt) const;

  private:
    bool matches_condition(const ASTNode *node) const;
    friend class Mado_Entries;
};

// ================ ENTRIES

class Mado_Entries {
  public:
    static Mado_Entries get_all(const Mado_Config *cfg, const char *main_dir);

    Mado_Entries &filter(const ASTNode *filter);
    void print(const Mado_Config *cfg, Mado_Output_Format fmt) const;
    void remove() const;
    size_t size() const { return entries_.size(); }

  private:
    std::vector<std::unique_ptr<Mado_Entry>> entries_;
};

// ================ LIFECYCLE

int mado_init(Mado_Config *cfg);
void mado_deinit();

// ================ REPO MANAGEMENT

int mado_templates_dir_init(const Mado_Config *cfg);
int mado_entries_dir_init(const Mado_Config *cfg, int force);
int mado_entry_create_dir_and_md(const Mado_Config *cfg, const char *main_dir, Mado_Output_Format fmt);
int mado_print_repo_info(const Mado_Config *cfg);

// ================ UTILS

int mado_parse_format(const char *format_str, Mado_Output_Format *fmt);

#endif
