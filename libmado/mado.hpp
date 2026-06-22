#ifndef MADO_HPP
#define MADO_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "ast.h"
}

// ================ TYPES

typedef enum {
    MADO_ERR_OK = 0,
    MADO_ERR_NOT_FOUND,
    MADO_ERR_IO,
    MADO_ERR_PERM,
    MADO_ERR_PARSE,
    MADO_ERR_INVALID_FORMAT,
    MADO_ERR_ALREADY_EXISTS,
    MADO_ERR_FOUND_ABOVE,
    MADO_ERR_TEMPLATE,
    MADO_ERR_HOOK,
    MADO_ERR_INTERNAL,
} Mado_Error;

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

typedef enum {
    MADO_SORT_ASC,
    MADO_SORT_DESC,
} Mado_Sort_Order;

struct Mado_Sort_Criterion {
    Mado_Entry_Field field;
    Mado_Sort_Order order;
};

struct Mado_AST_Deleter {
    AST_Node *node;
    ~Mado_AST_Deleter() { ast_free(node); }
};

// ================ CONFIG

struct Mado_Config {
    std::string main_dir_name;
    std::string templates_dir_name;
    std::string hooks_dir_name;
    std::string entry_file_name;
    std::string template_name;
    int max_header_lines;
    bool abs_paths;
    Mado_Entry_Field hide_fields;
    std::vector<Mado_Sort_Criterion> sort_criteria;
    Mado_Output_Format fmt;
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

    void print(const Mado_Config *cfg, std::ostream &os = std::cout) const;
    bool matches_condition(const AST_Node *filter) const;

    static std::pair<std::unique_ptr<Mado_Entry>, Mado_Error> create(const Mado_Config *cfg, const char *main_dir);
    static std::unique_ptr<Mado_Entry> parse(const Mado_Config *cfg, const char *entry_dir);
};

// ================ ENTRIES

class Mado_Entries {
  public:
    Mado_Entries &filter(const AST_Node *filter);
    Mado_Entries &sort(const Mado_Config *cfg);

    void print(const Mado_Config *cfg, std::ostream &os = std::cout) const;
    std::vector<std::string> remove() const;
    size_t size() const { return entries_.size(); }

    // iterators
    auto begin() { return entries_.begin(); }
    auto end() { return entries_.end(); }
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }

    static Mado_Entries get_all(const Mado_Config *cfg, const char *main_dir);

  private:
    std::vector<std::unique_ptr<Mado_Entry>> entries_;
};

// ================ LIFECYCLE

void mado_init_config(Mado_Config *cfg);

// ================ REPO MANAGEMENT

Mado_Error mado_entries_dir_init(const Mado_Config *cfg, int force);
Mado_Error mado_templates_dir_init(const Mado_Config *cfg);
Mado_Error mado_hooks_dir_init(const Mado_Config *cfg);
Mado_Error mado_print_repo_info(const Mado_Config *cfg, std::ostream &os = std::cout);

// ================ HOOKS

Mado_Error mado_run_hook(const Mado_Config *cfg, const char *hook_name);

// ================ UTILS

Mado_Error mado_parse_format(const char *format_str, Mado_Output_Format *fmt);
Mado_Error mado_parse_sort(const char *sort_str, std::vector<Mado_Sort_Criterion> *criteria);

// ================ ERROR MESSAGE

const char *mado_strerror(Mado_Error err);
int mado_print_error(Mado_Error err, const char *context, std::ostream &os = std::cerr);

#endif
