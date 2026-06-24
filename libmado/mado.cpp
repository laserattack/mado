#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "mado.hpp"

// ================ HELPERS

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

static void trim(std::string &s) {
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
}

// ================ CONFIG

void mado_init_config(Mado_Config *cfg) {
    cfg->main_dir_name = "MADO";
    cfg->templates_dir_name = ".templates";
    cfg->hooks_dir_name = ".hooks";
    cfg->entry_file_name = "MAIN";
    cfg->template_name = "task";
    cfg->max_header_lines = 30;
    cfg->hide_fields = Mado_Entry_Field::NONE;
    cfg->abs_paths = false;
    cfg->sort_criteria.clear();
    cfg->fmt = Mado_Output_Format::UNIX;
}

// ================ ERROR MESSAGE

const char *mado_strerror(Mado_Error err) {
    switch (err) {
    case Mado_Error::OK:
        return "success";
    case Mado_Error::NOT_FOUND:
        return "not found";
    case Mado_Error::IO:
        return "I/O error";
    case Mado_Error::PARSE:
        return "parse error";
    case Mado_Error::INVALID_FORMAT:
        return "invalid format";
    case Mado_Error::ALREADY_EXISTS:
        return "already exists";
    case Mado_Error::FOUND_ABOVE:
        return "found above or here";
    case Mado_Error::TEMPLATE:
        return "template error";
    case Mado_Error::INTERNAL:
        return "internal error";
    case Mado_Error::PERM:
        return "permission denied";
    case Mado_Error::HOOK:
        return "hook error";
    default:
        return "unknown error";
    }
}

// ================ PRINT HELPERS

static void print_json_string(const std::string &str, std::ostream &os) {
    os << '"';
    for (char c : str) {
        switch (c) {
        case '"':
            os << "\\\"";
            break;
        case '\\':
            os << "\\\\";
            break;
        default:
            os << c;
        }
    }
    os << '"';
}

// ================ ENTRY

static Mado_Error mado_entry_create(const Mado_Config *cfg, const char *entry_path, const char *template_name) {
    std::filesystem::path entry_md = std::filesystem::path(entry_path) / (cfg->entry_file_name + ".md");

    if (template_name) {
        std::string main_dir = find_dir_up(cfg->main_dir_name);
        if (main_dir.empty())
            return Mado_Error::NOT_FOUND;

        std::filesystem::path template_file = std::filesystem::path(main_dir) /
                                              cfg->templates_dir_name /
                                              (std::string(template_name) + ".md");

        if (std::filesystem::exists(template_file)) {
            std::filesystem::copy_file(template_file, entry_md, std::filesystem::copy_options::overwrite_existing);
            return Mado_Error::OK;
        }
        return Mado_Error::TEMPLATE;
    }

    std::ofstream f(entry_md);
    if (!f)
        return Mado_Error::IO;
    return Mado_Error::OK;
}

std::pair<std::unique_ptr<Mado_Entry>, Mado_Error> Mado_Entry::create(const Mado_Config *cfg, const char *main_dir) {
    time_t t = ::time(NULL);
    struct tm *tm = localtime(&t);
    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);

    std::filesystem::path entry_path = std::filesystem::path(main_dir) / dir_name;

    if (std::filesystem::exists(entry_path))
        return {nullptr, Mado_Error::ALREADY_EXISTS};
    if (!std::filesystem::create_directory(entry_path))
        return {nullptr, Mado_Error::IO};

    Mado_Error err = mado_entry_create(cfg, entry_path.c_str(), cfg->template_name.c_str());
    if (err != Mado_Error::OK)
        return {nullptr, err};

    return {parse(cfg, entry_path.c_str()), Mado_Error::OK};
}

std::unique_ptr<Mado_Entry> Mado_Entry::parse(const Mado_Config *cfg, const char *entry_dir) {
    static std::regex priority_value_regex("^[0-9]{1,3}$");
    static std::regex deadline_value_regex("^([0-9]{4}|[0-9]{6}|[0-9]{8}(T([0-9]{2}|[0-9]{4}|[0-9]{6})?)?)$");

    std::filesystem::path dir_path(entry_dir);
    std::string dir_name = dir_path.filename().string();

    auto entry_file = dir_path / (cfg->entry_file_name + ".md");

    std::ifstream f(entry_file);
    if (!f)
        return nullptr;

    auto entry = std::make_unique<Mado_Entry>();
    entry->priority = 0;
    entry->path = cfg->abs_paths ? entry_dir : std::filesystem::relative(entry_dir);
    entry->time = dir_name;
    entry->deadline = "99990000T000000";

    std::string line;
    int lines_processed = 0;

    while (lines_processed < cfg->max_header_lines && std::getline(f, line)) {
        lines_processed++;

        if (line.rfind("- NAME:", 0) == 0) {
            entry->name = line.substr(7);
            trim(entry->name);
        } else if (line.rfind("- PRIORITY:", 0) == 0) {
            std::string val = line.substr(11);
            trim(val);
            if (std::regex_match(val, priority_value_regex))
                entry->priority = std::stoi(val);
        } else if (line.rfind("- TAGS:", 0) == 0) {
            std::string tags_str = line.substr(7);
            trim(tags_str);
            if (!tags_str.empty()) {
                std::stringstream ss(tags_str);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    trim(token);
                    if (!token.empty())
                        entry->tags.push_back(token);
                }
            }
        } else if (line.rfind("- STATUS:", 0) == 0) {
            entry->status = line.substr(9);
            trim(entry->status);
        } else if (line.rfind("- DEADLINE:", 0) == 0) {
            std::string val = line.substr(11);
            trim(val);
            if (std::regex_match(val, deadline_value_regex))
                entry->deadline = val;
        }
    }

    if (entry->tags.empty())
        entry->tags.push_back("");
    return entry;
}

void Mado_Entry::print(const Mado_Config *cfg, std::ostream &os) const {
    Mado_Entry_Field hidden = cfg->hide_fields;
    Mado_Entry_Field shown = (Mado_Entry_Field)(Mado_Entry_Field::ALL & ~hidden);
    Mado_Output_Format fmt = cfg->fmt;

    if (fmt == Mado_Output_Format::ONLY_PATH) {
        os << path << "/" << cfg->entry_file_name << ".md\n";
        return;
    }

    if (fmt == Mado_Output_Format::JSONL) {
        os << "{";
        bool has_any = false;
        auto sep = [&]() {
            if (has_any)
                os << ",";
            has_any = true;
        };

        if (has_flag(shown, Mado_Entry_Field::TIME)) {
            sep();
            os << "\"time\":";
            print_json_string(time, os);
        }
        if (has_flag(shown, Mado_Entry_Field::NAME)) {
            sep();
            os << "\"name\":";
            print_json_string(name, os);
        }
        if (has_flag(shown, Mado_Entry_Field::PRIORITY)) {
            sep();
            os << "\"priority\":" << priority;
        }
        if (has_flag(shown, Mado_Entry_Field::DEADLINE)) {
            sep();
            os << "\"deadline\":";
            print_json_string(deadline, os);
        }
        if (has_flag(shown, Mado_Entry_Field::STATUS)) {
            sep();
            os << "\"status\":";
            print_json_string(status, os);
        }
        if (has_flag(shown, Mado_Entry_Field::TAGS)) {
            sep();
            os << "\"tags\":[";
            bool first_tag = true;
            for (const auto &tag : tags) {
                if (tag.empty())
                    continue;
                if (!first_tag)
                    os << ",";
                print_json_string(tag, os);
                first_tag = false;
            }
            os << "]";
        }
        if (has_flag(shown, Mado_Entry_Field::PATH)) {
            sep();
            os << "\"path\":\"" << path << "/" << cfg->entry_file_name << ".md\"";
        }
        os << "}\n";
        return;
    }

    // Mado_Output_Format::UNIX
    os << path << "/" << cfg->entry_file_name << ".md:1:";
    if (has_flag(shown, Mado_Entry_Field::TIME))
        os << " TIME:[" << time << "]";
    if (has_flag(shown, Mado_Entry_Field::NAME))
        os << " NAME:[" << name << "]";
    if (has_flag(shown, Mado_Entry_Field::PRIORITY))
        os << " PRIORITY:[" << priority << "]";
    if (has_flag(shown, Mado_Entry_Field::DEADLINE))
        os << " DEADLINE:[" << deadline << "]";
    if (has_flag(shown, Mado_Entry_Field::STATUS))
        os << " STATUS:[" << status << "]";
    if (has_flag(shown, Mado_Entry_Field::TAGS)) {
        os << " TAGS:[";
        bool first = true;
        for (const auto &tag : tags) {
            if (tag.empty())
                continue;
            if (!first)
                os << ",";
            os << tag;
            first = false;
        }
        os << "]";
    }
    os << "\n";
}

bool Mado_Entry::matches_condition(const AST_Node *filter) const {
    if (!filter)
        return true;
    switch (filter->type) {
    case NODE_ALL:
        return true;
    case NODE_BINARY_OP:
        switch (filter->binary.op) {
        case OP_AND:
            return matches_condition(filter->binary.left) && matches_condition(filter->binary.right);
        case OP_OR:
            return matches_condition(filter->binary.left) || matches_condition(filter->binary.right);
        default:
            return false;
        }
    case NODE_UNARY_OP:
        if (filter->unary.op == OP_NOT)
            return !matches_condition(filter->unary.expr);
        return false;
    case NODE_COMPARISON: {
        switch (filter->comparison.field) {
        case CMP_PRIORITY: {
            int cv = filter->comparison.value.int_value;
            switch (filter->comparison.cmp) {
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
            const char *ct = filter->comparison.value.str_value;
            for (const auto &tag : tags) {
                switch (filter->comparison.cmp) {
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
            return (filter->comparison.cmp == CMP_NE || filter->comparison.cmp == CMP_NTILDE);
        }
        case CMP_STATUS:
        case CMP_DEADLINE:
        case CMP_TIME:
        case CMP_NAME: {
            const std::string *field = nullptr;
            switch (filter->comparison.field) {
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
            const char *c = filter->comparison.value.str_value;
            if (field->empty())
                return false;
            switch (filter->comparison.cmp) {
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
    static std::regex entry_dir_regex("^[0-9]{8}T[0-9]{6}$");
    Mado_Entries result;
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

Mado_Entries &Mado_Entries::filter(const AST_Node *filter) {
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

Mado_Entries &Mado_Entries::sort(const Mado_Config *cfg) {
    if (cfg->sort_criteria.empty())
        return *this;

    auto compare_strings = [](const std::string &x, const std::string &y,
                              bool &less, bool &greater) {
        if (x < y)
            less = true;
        else if (x > y)
            greater = true;
    };

    auto comparator = [&](const std::unique_ptr<Mado_Entry> &a,
                          const std::unique_ptr<Mado_Entry> &b) -> bool {
        for (const auto &criterion : cfg->sort_criteria) {
            bool less = false, greater = false;
            switch (criterion.field) {
            case Mado_Entry_Field::TIME:
                compare_strings(a->time, b->time, less, greater);
                break;
            case Mado_Entry_Field::NAME:
                compare_strings(a->name, b->name, less, greater);
                break;
            case Mado_Entry_Field::PRIORITY:
                if (a->priority < b->priority)
                    less = true;
                else if (a->priority > b->priority)
                    greater = true;
                break;
            case Mado_Entry_Field::DEADLINE:
                compare_strings(a->deadline, b->deadline, less, greater);
                break;
            case Mado_Entry_Field::STATUS:
                compare_strings(a->status, b->status, less, greater);
                break;
            case Mado_Entry_Field::TAGS: {
                std::string tags_a, tags_b;
                for (const auto &tag : a->tags)
                    tags_a += tag;
                for (const auto &tag : b->tags)
                    tags_b += tag;
                compare_strings(tags_a, tags_b, less, greater);
                break;
            }
            default:
                break;
            }
            if (less)
                return criterion.order == Mado_Sort_Order::ASC;
            if (greater)
                return criterion.order == Mado_Sort_Order::DESC;
        }
        return false;
    };

    std::stable_sort(entries_.begin(), entries_.end(), comparator);
    return *this;
}

Mado_Error mado_parse_sort(const char *sort_str, std::vector<Mado_Sort_Criterion> *criteria) {
    if (!sort_str || !*sort_str)
        return Mado_Error::INVALID_FORMAT;

    std::string input(sort_str);
    std::stringstream ss(input);
    std::string token;

    static const struct {
        const char *name;
        Mado_Entry_Field field;
    } fields_for_parse_sort[] = {
        {"time", Mado_Entry_Field::TIME},
        {"name", Mado_Entry_Field::NAME},
        {"priority", Mado_Entry_Field::PRIORITY},
        {"deadline", Mado_Entry_Field::DEADLINE},
        {"status", Mado_Entry_Field::STATUS},
        {"tags", Mado_Entry_Field::TAGS},
    };
    static const int n_fields = sizeof(fields_for_parse_sort) / sizeof(fields_for_parse_sort[0]);

    while (std::getline(ss, token, ',')) {
        size_t start = token.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        size_t end = token.find_last_not_of(" \t");
        token = token.substr(start, end - start + 1);

        Mado_Sort_Criterion c;

        if (token[0] == '+') {
            c.order = Mado_Sort_Order::ASC;
            token = token.substr(1);
        } else if (token[0] == '-') {
            c.order = Mado_Sort_Order::DESC;
            token = token.substr(1);
        } else {
            c.order = Mado_Sort_Order::ASC;
        }

        int matches_count = 0;
        Mado_Entry_Field matched_field;
        for (int i = 0; i < n_fields; i++) {
            int field_len = (int)strlen(fields_for_parse_sort[i].name);
            if ((int)token.size() <= field_len &&
                strncmp(token.c_str(), fields_for_parse_sort[i].name, token.size()) == 0) {
                matched_field = fields_for_parse_sort[i].field;
                if ((int)token.size() == field_len) {
                    matches_count = 1;
                    break;
                }
                matches_count++;
            }
        }

        if (matches_count == 1)
            c.field = matched_field;
        else
            return Mado_Error::PARSE;

        criteria->push_back(c);
    }
    return Mado_Error::OK;
}

void Mado_Entries::print(const Mado_Config *cfg, std::ostream &os) const {
    for (auto &e : *this)
        e->print(cfg, os);
}

std::vector<std::string> Mado_Entries::remove() const {
    std::vector<std::string> removed;
    for (auto &e : *this) {
        std::filesystem::remove_all(e->path);
        removed.push_back(e->path);
    }
    return removed;
}

// ================ HOOKS

Mado_Error mado_run_hook(const Mado_Config *cfg, const char *hook_name) {
    std::string main_dir = find_dir_up(cfg->main_dir_name);
    if (main_dir.empty())
        return Mado_Error::NOT_FOUND;

    std::filesystem::path hook_path = std::filesystem::path(main_dir) / cfg->hooks_dir_name / hook_name;

    if (access(hook_path.c_str(), F_OK) != 0) // file exists?
        return Mado_Error::OK;

    if (std::filesystem::file_size(hook_path) == 0) // file empty?
        return Mado_Error::OK;

    if (access(hook_path.c_str(), X_OK) != 0) // can run?
        return Mado_Error::PERM;

    int ret = system(hook_path.c_str());
    if (ret != 0)
        return Mado_Error::HOOK;

    return Mado_Error::OK;
}

// ================ INIT

Mado_Error mado_hooks_dir_init(const Mado_Config *cfg) {
    std::string main_dir = find_dir_up(cfg->main_dir_name);
    if (main_dir.empty())
        return Mado_Error::NOT_FOUND;

    std::filesystem::path hooks_dir = std::filesystem::path(main_dir) / cfg->hooks_dir_name;

    if (!std::filesystem::exists(hooks_dir)) {
        if (!std::filesystem::create_directory(hooks_dir))
            return Mado_Error::IO;
    }

    static const char *hook_names[] = {
        "pre-new",
        "post-new",
        "pre-list",
        "post-list",
        "pre-remove",
        "post-remove",
        "pre-info",
        "post-info",
        NULL};

    for (int i = 0; hook_names[i]; i++) {
        auto path = hooks_dir / hook_names[i];
        if (!std::filesystem::exists(path)) {
            std::ofstream f(path);
            if (!f)
                return Mado_Error::IO;
        }
    }

    return Mado_Error::OK;
}

Mado_Error mado_templates_dir_init(const Mado_Config *cfg) {
    std::string main_dir = find_dir_up(cfg->main_dir_name);
    if (main_dir.empty())
        return Mado_Error::NOT_FOUND;

    std::filesystem::path templates_dir = std::filesystem::path(main_dir) / cfg->templates_dir_name;

    if (!std::filesystem::exists(templates_dir)) {
        if (!std::filesystem::create_directory(templates_dir))
            return Mado_Error::IO;
    }

    auto create_template = [&](const std::string &name, const std::string &content) -> Mado_Error {
        auto path = templates_dir / (name + ".md");
        if (!std::filesystem::exists(path)) {
            std::ofstream f(path);
            if (!f)
                return Mado_Error::IO;
            f << content;
        }
        return Mado_Error::OK;
    };

    Mado_Error err;
    err = create_template("task", "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n");
    if (err != Mado_Error::OK)
        return err;
    err = create_template("note", "- NAME:\n- TAGS:\n");
    if (err != Mado_Error::OK)
        return err;

    return Mado_Error::OK;
}

Mado_Error mado_entries_dir_init(const Mado_Config *cfg, int force) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path entries_dir = cwd / cfg->main_dir_name;

    if (!force) {
        std::string existing = find_dir_up(cfg->main_dir_name);
        if (!existing.empty())
            // found somewhere at the top of the tree and there is no force flag
            return Mado_Error::FOUND_ABOVE;
    }

    if (std::filesystem::exists(entries_dir))
        // exists directly in cwd, cannot be created even with force
        return Mado_Error::ALREADY_EXISTS;

    if (!std::filesystem::create_directory(entries_dir))
        return Mado_Error::IO;

    return Mado_Error::OK;
}

Mado_Error mado_print_repo_info(const Mado_Config *cfg, std::ostream &os) {
    std::string main_dir = find_dir_up(cfg->main_dir_name);
    if (!cfg->abs_paths) {
        main_dir = std::filesystem::relative(std::filesystem::path(main_dir));
    }
    if (main_dir.empty()) {
        os << "No mado repository here\n";
        return Mado_Error::NOT_FOUND;
    }
    auto entries = Mado_Entries::get_all(cfg, main_dir.c_str());

    os << "Main directory: " << main_dir << "\n";
    os << "Entries count: " << entries.size() << "\n";

    std::map<std::string, int> status_counts;
    std::map<std::string, int> tag_counts;

    for (auto &e : entries) {
        if (!e->status.empty())
            status_counts[e->status]++;
        for (auto &tag : e->tags) {
            if (!tag.empty())
                tag_counts[tag]++;
        }
    }

    if (!status_counts.empty()) {
        os << "Statuses:\n";
        for (auto &[status, count] : status_counts)
            os << "  " << status << ": " << count << "\n";
    }

    if (!tag_counts.empty()) {
        os << "Tags:\n";
        for (auto &[tag, count] : tag_counts)
            os << "  " << tag << ": " << count << "\n";
    }

    return Mado_Error::OK;
}

Mado_Error mado_parse_format(const char *format_str, Mado_Output_Format *fmt) {
    if (strcmp(format_str, "path") == 0)
        *fmt = Mado_Output_Format::ONLY_PATH;
    else if (strcmp(format_str, "unix") == 0)
        *fmt = Mado_Output_Format::UNIX;
    else if (strcmp(format_str, "jsonl") == 0)
        *fmt = Mado_Output_Format::JSONL;
    else
        return Mado_Error::INVALID_FORMAT;
    return Mado_Error::OK;
}

int mado_print_error(Mado_Error err, const char *context, std::ostream &os) {
    if (err != Mado_Error::OK) {
        os << "Mado error (" << context << "): " << mado_strerror(err) << "\n";
        return -1;
    }
    return 0;
}
