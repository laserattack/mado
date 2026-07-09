#ifndef MADO_HPP
#define MADO_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "ast.h"
}

// make auto-free ast ptr
inline auto make_ast_ptr(AST_Node *node) {
    return std::unique_ptr<AST_Node, decltype(&ast_free)>(node, ast_free);
}

// ================ TYPES

enum class Mado_Error {
    OK = 0,
    NOT_FOUND,
    IO,
    PARSE,
    INVALID_FORMAT,
    ALREADY_EXISTS,
    FOUND_ABOVE,
    TEMPLATE,
    INTERNAL,
};

enum class Mado_Output_Format {
    DEFAULT = 1,
    ONLY_PATH,
    JSONL,
};

enum class Mado_Entry_Field {
    NONE,
    NAME = 1 << 0,
    TIME = 1 << 1,
    DEADLINE = 1 << 2,
    PRIORITY = 1 << 3,
    STATUS = 1 << 4,
    TAGS = 1 << 5,
    PATH = 1 << 6,
    ALL = NAME | TIME | DEADLINE | PRIORITY | STATUS | TAGS | PATH
};

constexpr Mado_Entry_Field operator|(Mado_Entry_Field a, Mado_Entry_Field b) {
    return static_cast<Mado_Entry_Field>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr Mado_Entry_Field operator&(Mado_Entry_Field a, Mado_Entry_Field b) {
    return static_cast<Mado_Entry_Field>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr Mado_Entry_Field operator~(Mado_Entry_Field a) {
    return static_cast<Mado_Entry_Field>(~static_cast<int>(a));
}

constexpr bool has_flag(Mado_Entry_Field value, Mado_Entry_Field flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

constexpr Mado_Entry_Field &operator|=(Mado_Entry_Field &a, Mado_Entry_Field b) {
    return a = a | b;
}

constexpr Mado_Entry_Field &operator&=(Mado_Entry_Field &a, Mado_Entry_Field b) {
    return a = a & b;
}

enum class Mado_Sort_Order {
    ASC,
    DESC,
};

struct Mado_Sort_Criterion {
    Mado_Entry_Field field;
    Mado_Sort_Order order;
};

// ================ CONFIG

struct Mado_Config {
    std::string main_dir_name;
    std::string templates_dir_name;
    std::string entry_file_name;
    std::string template_name;
    int max_header_lines;
    bool abs_paths;
    bool case_insensitive_search;
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
    bool matches_condition(const Mado_Config *cfg, const AST_Node *filter) const;

    static std::pair<std::unique_ptr<Mado_Entry>, Mado_Error> create(const Mado_Config *cfg, const char *main_dir);
    static std::unique_ptr<Mado_Entry> parse(const Mado_Config *cfg, const char *entry_dir);
};

// ================ ENTRIES

class Mado_Entries {
  public:
    Mado_Entries &filter(const Mado_Config *cfg, const AST_Node *filter);
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

std::pair<std::string, Mado_Error> mado_main_dir_init(const Mado_Config *cfg, int force);
Mado_Error mado_templates_dir_init(const Mado_Config *cfg);
Mado_Error mado_print_repo_info(const Mado_Config *cfg, std::ostream &os = std::cout);

// ================ UTILS

Mado_Error mado_parse_format(const char *format_str, Mado_Output_Format *fmt);
Mado_Error mado_parse_sort(const char *sort_str, std::vector<Mado_Sort_Criterion> *criteria);

// ================ ERROR MESSAGE

const char *mado_strerror(Mado_Error err);
int mado_print_error(Mado_Error err, const char *context, std::ostream &os = std::cerr);

#endif
