#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

extern "C" {
#define UTIL_IMPL
#include "utils/util.h"

#define REGEX_IMPL
#include "utils/regex.h"
}

#include "mado.hpp"

static std::string find_dir_up(const std::string &dir_name) {
    auto current = std::filesystem::current_path();
    while (true) {
        auto test = current / dir_name;
        if (std::filesystem::exists(test))
            return test.string();
        auto parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }
    return {};
}

// ================ CONFIG

static void mado_init_config(Mado_Config *cfg) {
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

static int mado_init_regexes() {
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

static void mado_free_regexes() {
    if (!g_regex_initialized)
        return;
    regfree(&g_entry_dir_regex);
    regfree(&g_name_regex);
    regfree(&g_priority_regex);
    regfree(&g_tags_regex);
    regfree(&g_status_regex);
    regfree(&g_deadline_regex);
}

// ================ PRINT HELPERS

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

static void print_json_string(const std::string &str) {
    print_json_string(str.c_str());
}

// ================ ENTRY

std::unique_ptr<Mado_Entry> Mado_Entry::parse(const Mado_Config *cfg, const char *entry_dir) {
    std::filesystem::path dir_path(entry_dir);
    std::string dir_name = dir_path.filename().string();

    auto entry_file = dir_path / (cfg->entry_file_name + ".md");

    std::ifstream f(entry_file);
    if (!f)
        return nullptr;

    auto entry = std::make_unique<Mado_Entry>();
    entry->priority = 0;
    entry->path = entry_dir;
    entry->time = dir_name;
    entry->deadline = "99990000T000000";

    std::string line;
    int lines_processed = 0;

    while (lines_processed < cfg->max_header_lines && std::getline(f, line)) {
        lines_processed++;

        char *value;
        if ((value = regex_extract_first_group(line.c_str(), &g_name_regex)) != NULL) {
            entry->name = trim(value);
            free(value);
        } else if ((value = regex_extract_first_group(line.c_str(), &g_priority_regex)) != NULL) {
            entry->priority = atoi(value);
            free(value);
        } else if ((value = regex_extract_first_group(line.c_str(), &g_tags_regex)) != NULL) {
            char *tags_str = trim(value);
            if (tags_str && *tags_str) {
                char *saveptr;
                char *token = strtok_r(tags_str, ",", &saveptr);
                while (token) {
                    char *clean = trim(token);
                    if (*clean)
                        entry->tags.push_back(clean);
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            free(value);
        } else if ((value = regex_extract_first_group(line.c_str(), &g_status_regex)) != NULL) {
            entry->status = trim(value);
            free(value);
        } else if ((value = regex_extract_first_group(line.c_str(), &g_deadline_regex)) != NULL) {
            entry->deadline = trim(value);
            free(value);
        }
    }

    if (entry->tags.empty())
        entry->tags.push_back("");
    return entry;
}

void Mado_Entry::print(const Mado_Config *cfg, Mado_Output_Format fmt) const {
    Mado_Entry_Field hidden = cfg->hide_fields;
    Mado_Entry_Field shown = (Mado_Entry_Field)(MADO_FIELD_ALL & ~hidden);

    if (fmt == MADO_FMT_ONLY_PATH) {
        printf("%s/%s.md\n", path.c_str(), cfg->entry_file_name.c_str());
        return;
    }

    if (fmt == MADO_FMT_JSONL) {
        printf("{");
        int has_any = 0;
        if (shown & MADO_FIELD_TIME) {
            if (has_any)
                printf(",");
            printf("\"time\":");
            print_json_string(time);
            has_any = 1;
        }
        if (shown & MADO_FIELD_NAME) {
            if (has_any)
                printf(",");
            printf("\"name\":");
            print_json_string(name);
            has_any = 1;
        }
        if (shown & MADO_FIELD_PRIORITY) {
            if (has_any)
                printf(",");
            printf("\"priority\":%d", priority);
            has_any = 1;
        }
        if (shown & MADO_FIELD_DEADLINE) {
            if (has_any)
                printf(",");
            printf("\"deadline\":");
            print_json_string(deadline);
            has_any = 1;
        }
        if (shown & MADO_FIELD_STATUS) {
            if (has_any)
                printf(",");
            printf("\"status\":");
            print_json_string(status);
            has_any = 1;
        }
        if (shown & MADO_FIELD_TAGS) {
            if (has_any)
                printf(",");
            printf("\"tags\":[");
            bool first_tag = true;
            for (size_t j = 0; j < tags.size(); j++) {
                if (tags[j].empty())
                    continue;
                if (!first_tag)
                    printf(",");
                print_json_string(tags[j]);
                first_tag = false;
            }
            printf("]");
            has_any = 1;
        }
        if (shown & MADO_FIELD_PATH) {
            if (has_any)
                printf(",");
            printf("\"path\":\"%s/%s.md\"", path.c_str(), cfg->entry_file_name.c_str());
            has_any = 1;
        }
        printf("}\n");
        return;
    }

    if (fmt == MADO_FMT_UNIX) {
        printf("%s/%s.md:1:1:", path.c_str(), cfg->entry_file_name.c_str());
        if (shown & MADO_FIELD_TIME)
            printf(" TIME:[%s]", time.c_str());
        if (shown & MADO_FIELD_NAME)
            printf(" NAME:[%s]", name.c_str());
        if (shown & MADO_FIELD_PRIORITY)
            printf(" PRIORITY:[%d]", priority);
        if (shown & MADO_FIELD_DEADLINE)
            printf(" DEADLINE:[%s]", deadline.c_str());
        if (shown & MADO_FIELD_STATUS)
            printf(" STATUS:[%s]", status.c_str());
        if (shown & MADO_FIELD_TAGS) {
            printf(" TAGS:[");
            for (size_t j = 0; j < tags.size(); j++) {
                if (tags[j].empty())
                    continue;
                printf("%s", tags[j].c_str());
                if (j < tags.size() - 1 && !tags[j + 1].empty())
                    printf(",");
            }
            printf("]");
        }
        printf("\n");
    }
}

bool Mado_Entry::matches_condition(const ASTNode *node) const {
    if (!node)
        return true;
    switch (node->type) {
    case NODE_ALL:
        return true;
    case NODE_BINARY_OP:
        switch (node->binary.op) {
        case OP_AND:
            return matches_condition(node->binary.left) && matches_condition(node->binary.right);
        case OP_OR:
            return matches_condition(node->binary.left) || matches_condition(node->binary.right);
        default:
            return false;
        }
    case NODE_UNARY_OP:
        if (node->unary.op == OP_NOT)
            return !matches_condition(node->unary.expr);
        return false;
    case NODE_COMPARISON: {
        switch (node->comparison.field) {
        case CMP_PRIORITY: {
            int cv = node->comparison.value.int_value;
            switch (node->comparison.cmp) {
            case CMP_GT:
                return priority > cv;
            case CMP_LT:
                return priority < cv;
            case CMP_GE:
                return priority >= cv;
            case CMP_LE:
                return priority <= cv;
            case CMP_TILDE:
            case CMP_EQ:
                return priority == cv;
            case CMP_NTILDE:
            case CMP_NE:
                return priority != cv;
            default:
                return false;
            }
        }
        case CMP_TAG: {
            const char *ct = node->comparison.value.str_value;
            for (const auto &tag : tags) {
                switch (node->comparison.cmp) {
                case CMP_EQ:
                    if (tag == ct)
                        return true;
                    break;
                case CMP_NE:
                    if (tag == ct)
                        return false;
                    break;
                case CMP_TILDE:
                    if (tag.find(ct) != std::string::npos)
                        return true;
                    break;
                case CMP_NTILDE:
                    if (tag.find(ct) != std::string::npos)
                        return false;
                    break;
                case CMP_GT:
                    if (tag > ct)
                        return true;
                    break;
                case CMP_LT:
                    if (tag < ct)
                        return true;
                    break;
                case CMP_GE:
                    if (tag >= ct)
                        return true;
                    break;
                case CMP_LE:
                    if (tag <= ct)
                        return true;
                    break;
                default:
                    break;
                }
            }
            return (node->comparison.cmp == CMP_NE || node->comparison.cmp == CMP_NTILDE);
        }
        case CMP_STATUS:
        case CMP_DEADLINE:
        case CMP_TIME:
        case CMP_NAME: {
            const std::string *field = nullptr;
            switch (node->comparison.field) {
            case CMP_STATUS:
                field = &status;
                break;
            case CMP_DEADLINE:
                field = &deadline;
                break;
            case CMP_TIME:
                field = &time;
                break;
            case CMP_NAME:
                field = &name;
                break;
            default:
                return false;
            }
            const char *c = node->comparison.value.str_value;
            if (field->empty())
                return false;
            switch (node->comparison.cmp) {
            case CMP_EQ:
                return *field == c;
            case CMP_NE:
                return *field != c;
            case CMP_TILDE:
                return field->find(c) != std::string::npos;
            case CMP_NTILDE:
                return field->find(c) == std::string::npos;
            case CMP_GT:
                return *field > c;
            case CMP_LT:
                return *field < c;
            case CMP_GE:
                return *field >= c;
            case CMP_LE:
                return *field <= c;
            default:
                return false;
            }
        }
        default:
            return false;
        }
    }
    default:
        return false;
    }
}

// ================ ENTRIES

Mado_Entries Mado_Entries::get_all(const Mado_Config *cfg, const char *main_dir) {
    Mado_Entries result;

    std::regex entry_dir_regex(R"(^[0-9]{8}T[0-9]{6}$)");
    std::filesystem::path dir_path(main_dir);

    if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path))
        return result;

    for (const auto &dirent : std::filesystem::directory_iterator(dir_path)) {
        if (!dirent.is_directory())
            continue;

        std::string dir_name = dirent.path().filename().string();
        if (!std::regex_match(dir_name, entry_dir_regex))
            continue;

        auto entry_file = dirent.path() / (cfg->entry_file_name + ".md");
        if (!std::filesystem::exists(entry_file))
            continue;

        auto entry = Mado_Entry::parse(cfg, dirent.path().c_str());
        if (entry)
            result.entries_.push_back(std::move(entry));
    }

    return result;
}

Mado_Entries &Mado_Entries::filter(const ASTNode *filter) {
    if (!filter)
        return *this;

    auto it = entries_.begin();
    while (it != entries_.end()) {
        if ((*it)->matches_condition(filter))
            ++it;
        else
            it = entries_.erase(it);
    }
    return *this;
}

void Mado_Entries::print(const Mado_Config *cfg, Mado_Output_Format fmt) const {
    for (auto &e : entries_)
        e->print(cfg, fmt);
}

void Mado_Entries::remove() const {
    for (auto &e : entries_) {
        std::filesystem::remove_all(e->path.c_str());
        printf("Removed: %s\n", e->path.c_str());
    }
}

// ================ INIT

int mado_templates_dir_init(const Mado_Config *cfg) {
    std::string main_dir = find_dir_up(cfg->main_dir_name.c_str());
    if (main_dir.empty()) {
        fprintf(stderr, "Error: entries directory '%s' not found\n", cfg->main_dir_name.c_str());
        return -1;
    }

    std::filesystem::path templates_dir = std::filesystem::path(main_dir) / cfg->templates_dir_name;

    if (!std::filesystem::exists(templates_dir)) {
        if (!std::filesystem::create_directory(templates_dir)) {
            fprintf(stderr, "Error: failed to create templates directory: %s\n", templates_dir.c_str());
            return -1;
        }
        printf("Created templates directory: %s\n", templates_dir.c_str());
    } else {
        printf("Templates directory already exists: %s\n", templates_dir.c_str());
    }

    auto create_template = [&](const std::string &name, const std::string &content) {
        auto path = templates_dir / (name + ".md");
        if (!std::filesystem::exists(path)) {
            FILE *f = fopen(path.c_str(), "w");
            if (f) {
                fprintf(f, "%s", content.c_str());
                fclose(f);
                printf("Created %s template: %s\n", name.c_str(), path.c_str());
            } else {
                fprintf(stderr, "Error: failed to create %s template: %s\n", name.c_str(), path.c_str());
                return -1;
            }
        } else {
            printf("%s template already exists: %s\n", name.c_str(), path.c_str());
        }
        return 0;
    };

    if (create_template("task", "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n") != 0)
        return -1;
    if (create_template("note", "- NAME:\n- TAGS:\n") != 0)
        return -1;

    return 0;
}

int mado_entries_dir_init(const Mado_Config *cfg, int force) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path entries_dir = cwd / cfg->main_dir_name;

    if (!force) {
        std::string entries_dir_existing = find_dir_up(cfg->main_dir_name.c_str());
        if (!entries_dir_existing.empty()) {
            printf("Entries directory already exists: %s\n", entries_dir_existing.c_str());
            if (std::filesystem::path(entries_dir_existing) != entries_dir)
                printf("Hint: use -F to force initialization in current directory\n");
            return 0;
        }
    }

    if (std::filesystem::exists(entries_dir)) {
        printf("Entries directory already exists: %s\n", entries_dir.c_str());
        if (force)
            printf("Hint: -F has no effect because '%s' already exists in this location\n", cfg->main_dir_name.c_str());
    } else {
        if (std::filesystem::create_directory(entries_dir)) {
            printf("Created entries directory: %s\n", entries_dir.c_str());
        } else {
            fprintf(stderr, "Error: failed to create entries directory: %s\n", entries_dir.c_str());
            return -1;
        }
    }

    return 0;
}

static int mado_entry_create(const Mado_Config *cfg, const char *entry_path, const char *template_name) {
    std::filesystem::path entry_md = std::filesystem::path(entry_path) / (cfg->entry_file_name + ".md");

    if (template_name) {
        std::string main_dir = find_dir_up(cfg->main_dir_name.c_str());
        if (main_dir.empty()) {
            fprintf(stderr, "Error: entries directory '%s' not found\n", cfg->main_dir_name.c_str());
            return -1;
        }

        std::filesystem::path template_file = std::filesystem::path(main_dir) / cfg->templates_dir_name / (std::string(template_name) + ".md");

        if (std::filesystem::exists(template_file)) {
            std::filesystem::copy_file(template_file, entry_md, std::filesystem::copy_options::overwrite_existing);
            return 0;
        } else {
            fprintf(stderr, "Error: template '%s' was not found\n", template_name);
            return -1;
        }
    }

    FILE *f = fopen(entry_md.c_str(), "w");
    if (!f) {
        fprintf(stderr, "Error: failed to create %s.md in: %s\n", cfg->entry_file_name.c_str(), entry_path);
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

    std::filesystem::path entry_path = std::filesystem::path(main_dir) / dir_name;

    if (std::filesystem::exists(entry_path)) {
        fprintf(stderr, "Error: entry directory already exists\n");
        return -1;
    }
    if (!std::filesystem::create_directory(entry_path)) {
        fprintf(stderr, "Error: failed to create entry directory\n");
        return -1;
    }
    if (mado_entry_create(cfg, entry_path.c_str(), cfg->template_name.c_str()) != 0)
        return -1;

    Mado_Entry entry;
    entry.path = entry_path.string();
    entry.name = "";
    entry.status = "";
    entry.time = dir_name;
    entry.deadline = "";
    entry.print(cfg, fmt);
    return 0;
}

int mado_print_repo_info(const Mado_Config *cfg) {
    std::string main_dir = find_dir_up(cfg->main_dir_name);
    if (main_dir.empty()) {
        printf("No mado repository here\n");
        return 0;
    }
    auto entries = Mado_Entries::get_all(cfg, main_dir.c_str());
    printf("Main directory: %s\n", main_dir.c_str());
    printf("Entries count:  %zu\n", entries.size());
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

int mado_init(Mado_Config *cfg) {
    mado_init_config(cfg);
    return mado_init_regexes();
}

void mado_deinit() {
    mado_free_regexes();
}
