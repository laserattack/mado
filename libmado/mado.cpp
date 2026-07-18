#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mado.hpp"

extern "C" {
#include "utils.h" // impl in lexer.l
}

static void trim(std::string &s) {
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
}

// ================ CONFIG

void mado_init_config(Mado_Config *cfg) {
    cfg->main_dir_name = "MADO";
    cfg->templates_dir_name = ".templates";
    cfg->entry_file_name = "MAIN";
    cfg->template_name = "task";
    cfg->max_header_lines = 30;
    cfg->hide_fields = Mado_Entry_Field::NONE;
    cfg->abs_paths = false;
    cfg->case_insensitive_search = false;
    cfg->sort_criteria.clear();
    cfg->fmt = Mado_Output_Format::DEFAULT;
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

std::pair<std::filesystem::path, Mado_Error>
mado_find_main_dir(const Mado_Config *cfg) {

    auto current = std::filesystem::current_path();
    while (true) {
        auto test = current / cfg->main_dir_name;
        if (std::filesystem::exists(test))
            return {test, Mado_Error::OK};
        auto parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }
    return {{}, Mado_Error::NOT_FOUND};
}

static Mado_Error
mado_entry_create(const Mado_Config *cfg,
                  const std::filesystem::path &entry_path,
                  const std::string &template_name) {

    std::filesystem::path entry_md = entry_path / (cfg->entry_file_name + ".md");

    if (!template_name.empty()) {
        auto [main_dir, err] = mado_find_main_dir(cfg);
        if (err != Mado_Error::OK)
            return err;

        std::filesystem::path template_file = main_dir / cfg->templates_dir_name / (template_name + ".md");

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

std::pair<std::unique_ptr<Mado_Entry>, Mado_Error>
Mado_Entry::create(const Mado_Config *cfg,
                   const std::filesystem::path &main_dir) {

    time_t t = ::time(nullptr);
    struct tm *tm = localtime(&t);
    char dir_name[16];
    strftime(dir_name, sizeof(dir_name), "%Y%m%dT%H%M%S", tm);

    std::filesystem::path entry_path = main_dir / dir_name;

    if (std::filesystem::exists(entry_path))
        return {nullptr, Mado_Error::ALREADY_EXISTS};
    if (!std::filesystem::create_directory(entry_path))
        return {nullptr, Mado_Error::IO};

    Mado_Error err = mado_entry_create(cfg, entry_path, cfg->template_name);
    if (err != Mado_Error::OK)
        return {nullptr, err};

    return {parse(cfg, entry_path), Mado_Error::OK};
}

std::unique_ptr<Mado_Entry>
Mado_Entry::parse(const Mado_Config *cfg,
                  const std::filesystem::path &entry_dir) {

    auto is_valid_priority = [](const std::string &s) {
        return !s.empty() && s.size() <= 3 && std::all_of(s.begin(), s.end(), ::isdigit);
    };
    auto is_valid_deadline = [](const std::string &s) {
        if (s.empty() || s.size() > 15)
            return false;

        // date part
        size_t pos = 0;
        while (pos < s.size() && ::isdigit(s[pos]) && pos < 8)
            ++pos;

        if (pos != 4 && pos != 6 && pos != 8)
            return false;

        if (pos == s.size())
            return true;

        // time part
        if (s[pos] != 'T')
            return false;
        ++pos;

        if (pos == s.size())
            return true;

        size_t time_digits = 0;
        while (pos < s.size() && ::isdigit(s[pos]) && time_digits < 6) {
            ++pos;
            ++time_digits;
        }
        return pos == s.size() && (time_digits == 2 || time_digits == 4 || time_digits == 6);
    };

    std::string dir_name = entry_dir.filename().string();

    auto entry_file = entry_dir / (cfg->entry_file_name + ".md");

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
            if (is_valid_priority(val))
                entry->priority = std::stoi(val);
        } else if (line.rfind("- TAGS:", 0) == 0) {
            std::string tags_str = line.substr(7);
            trim(tags_str);
            if (!tags_str.empty()) {
                std::stringstream ss(tags_str);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    trim(token);
                    entry->tags.push_back(token);
                }
            }
        } else if (line.rfind("- STATUS:", 0) == 0) {
            entry->status = line.substr(9);
            trim(entry->status);
        } else if (line.rfind("- DEADLINE:", 0) == 0) {
            std::string val = line.substr(11);
            trim(val);
            if (is_valid_deadline(val))
                entry->deadline = val;
        }
    }

    if (entry->tags.empty())
        entry->tags.push_back("");
    return entry;
}

void Mado_Entry::print(const Mado_Config *cfg, std::ostream &os) const {
    Mado_Entry_Field hidden = cfg->hide_fields;
    Mado_Entry_Field shown = Mado_Entry_Field::ALL & ~hidden;
    Mado_Output_Format fmt = cfg->fmt;

    if (fmt == Mado_Output_Format::ONLY_PATH) {
        os << path.string() << "/" << cfg->entry_file_name << ".md\n";
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
                if (!first_tag)
                    os << ",";
                print_json_string(tag, os);
                first_tag = false;
            }
            os << "]";
        }
        if (has_flag(shown, Mado_Entry_Field::PATH)) {
            sep();
            os << "\"path\":\"" << path.string() << "/" << cfg->entry_file_name << ".md\"";
        }
        os << "}\n";
        return;
    }

    // Mado_Output_Format::DEFAULT
    os << path.string() << "/" << cfg->entry_file_name << ".md:1:";
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
            if (!first)
                os << ",";
            os << tag;
            first = false;
        }
        os << "]";
    }
    os << "\n";
}

// interpreter
bool Mado_Entry::matches_condition(const Mado_Config *cfg, const AST_Node *filter) const {
    if (!filter)
        return true;

    auto normalize_string_field = [cfg](const std::string &s) -> std::string {
        if (!cfg->case_insensitive_search)
            return s;
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    };

    switch (filter->type) {
    case NODE_ALL:
        return true;
    case NODE_BINARY_OP:
        switch (filter->binary.op) {
        case OP_AND:
            return matches_condition(cfg, filter->binary.left) && matches_condition(cfg, filter->binary.right);
        case OP_OR:
            return matches_condition(cfg, filter->binary.left) || matches_condition(cfg, filter->binary.right);
        default:
            return false;
        }
    case NODE_UNARY_OP:
        if (filter->unary.op == OP_NOT)
            return !matches_condition(cfg, filter->unary.expr);
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
            const std::string ct = normalize_string_field(filter->comparison.value.str_value);

            for (const auto &tag : tags) {
                const std::string tag_normalized = normalize_string_field(tag);

                switch (filter->comparison.cmp) {
                case CMP_EQ:
                    if (tag_normalized == ct)
                        return true;
                    break;
                case CMP_NE:
                    if (tag_normalized != ct)
                        return true;
                    break;
                case CMP_TILDE:
                    if (tag_normalized.find(ct) != std::string::npos)
                        return true;
                    break;
                case CMP_NTILDE:
                    if (tag_normalized.find(ct) == std::string::npos)
                        return true;
                    break;
                case CMP_GT:
                    if (tag_normalized > ct)
                        return true;
                    break;
                case CMP_LT:
                    if (tag_normalized < ct)
                        return true;
                    break;
                case CMP_GE:
                    if (tag_normalized >= ct)
                        return true;
                    break;
                case CMP_LE:
                    if (tag_normalized <= ct)
                        return true;
                    break;
                default:
                    break;
                }
            }
            return false;
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

            const std::string c = normalize_string_field(filter->comparison.value.str_value);
            const std::string field_normalized = normalize_string_field(*field);

            switch (filter->comparison.cmp) {
            case CMP_EQ:
                return field_normalized == c;
            case CMP_NE:
                return field_normalized != c;
            case CMP_TILDE:
                return field_normalized.find(c) != std::string::npos;
            case CMP_NTILDE:
                return field_normalized.find(c) == std::string::npos;
            case CMP_GT:
                return field_normalized > c;
            case CMP_LT:
                return field_normalized < c;
            case CMP_GE:
                return field_normalized >= c;
            case CMP_LE:
                return field_normalized <= c;
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

Mado_Entries
Mado_Entries::get_all(const Mado_Config *cfg,
                      const std::filesystem::path &main_dir) {

    auto is_entry_dir = [](const std::string &s) {
        return s.size() == 15 &&
               std::all_of(s.begin(), s.begin() + 8, ::isdigit) &&
               s[8] == 'T' &&
               std::all_of(s.begin() + 9, s.end(), ::isdigit);
    };

    Mado_Entries result;

    if (!std::filesystem::exists(main_dir) || !std::filesystem::is_directory(main_dir))
        return result;

    for (const auto &dirent : std::filesystem::directory_iterator(main_dir)) {
        if (!dirent.is_directory())
            continue;
        std::string dir_name = dirent.path().filename().string();
        if (!is_entry_dir(dir_name))
            continue;
        auto entry_file = dirent.path() / (cfg->entry_file_name + ".md");
        if (!std::filesystem::exists(entry_file))
            continue;
        auto entry = Mado_Entry::parse(cfg, dirent.path());
        if (entry)
            result.entries_.push_back(std::move(entry));
    }
    return result;
}

Mado_Entries &Mado_Entries::filter(const Mado_Config *cfg, const AST_Node *filter) {
    if (!filter)
        return *this;
    auto it = entries_.begin();
    while (it != entries_.end()) {
        if ((*it)->matches_condition(cfg, filter))
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

std::pair<std::vector<Mado_Sort_Criterion>, Mado_Error>
mado_parse_sort(const std::string &sort_str) {

    if (sort_str.empty())
        return {{}, Mado_Error::INVALID_FORMAT};

    std::stringstream ss(sort_str);
    std::string token;
    std::vector<Mado_Sort_Criterion> criteria;

    static const struct keyword_entry fields_for_parse_sort[] = {
        {"time", static_cast<int>(Mado_Entry_Field::TIME)},
        {"name", static_cast<int>(Mado_Entry_Field::NAME)},
        {"priority", static_cast<int>(Mado_Entry_Field::PRIORITY)},
        {"deadline", static_cast<int>(Mado_Entry_Field::DEADLINE)},
        {"status", static_cast<int>(Mado_Entry_Field::STATUS)},
        {"tags", static_cast<int>(Mado_Entry_Field::TAGS)},
    };
    static const int n_fields = sizeof(fields_for_parse_sort) / sizeof(fields_for_parse_sort[0]);

    while (std::getline(ss, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(), is_whitespace), token.end());
        if (token.empty())
            continue;

        Mado_Sort_Criterion c;
        c.order = Mado_Sort_Order::ASC;

        if (token[0] == '+' || token[0] == '-') {
            if (token[0] == '-')
                c.order = Mado_Sort_Order::DESC;
            token.erase(0, 1);
        }

        int field_token = lookup_keyword(token.c_str(), fields_for_parse_sort, n_fields);
        if (field_token > 0) {
            c.field = static_cast<Mado_Entry_Field>(field_token);
        } else {
            return {{}, Mado_Error::PARSE};
        }

        criteria.push_back(c);
    }
    return {criteria, Mado_Error::OK};
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

Mado_Error mado_templates_dir_init(const Mado_Config *cfg) {
    auto [main_dir, err] = mado_find_main_dir(cfg);
    if (err != Mado_Error::OK)
        return err;

    std::filesystem::path templates_dir = main_dir / cfg->templates_dir_name;

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

    err = create_template("task", "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n");
    if (err != Mado_Error::OK)
        return err;
    err = create_template("note", "- NAME:\n- TAGS:\n");
    if (err != Mado_Error::OK)
        return err;

    return Mado_Error::OK;
}

std::pair<std::filesystem::path, Mado_Error>
mado_main_dir_init(const Mado_Config *cfg, int force) {

    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path entries_dir = cwd / cfg->main_dir_name;

    if (!force) {
        auto [existing, err] = mado_find_main_dir(cfg);
        if (err == Mado_Error::OK)
            // found somewhere at the top of the tree and there is no force flag
            return {existing, Mado_Error::FOUND_ABOVE};
    }

    if (std::filesystem::exists(entries_dir))
        // exists directly in cwd, cannot be created even with force
        return {entries_dir, Mado_Error::ALREADY_EXISTS};

    if (!std::filesystem::create_directory(entries_dir))
        return {"", Mado_Error::IO};

    return {entries_dir, Mado_Error::OK};
}

Mado_Error mado_print_repo_info(const Mado_Config *cfg, std::ostream &os) {
    auto [main_dir_path, err] = mado_find_main_dir(cfg);
    if (err != Mado_Error::OK)
        return err;

    main_dir_path = cfg->abs_paths
                        ? main_dir_path
                        : std::filesystem::relative(main_dir_path);

    auto entries = Mado_Entries::get_all(cfg, main_dir_path);

    if (cfg->fmt == Mado_Output_Format::JSONL) {
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

        os << "{\"main_dir\":";
        print_json_string(main_dir_path.string(), os);
        os << ",\"entries_count\":" << entries.size() << ",\"statuses\":{";

        bool first_status = true;
        for (auto &[status, count] : status_counts) {
            if (!first_status)
                os << ",";
            print_json_string(status, os);
            os << ":" << count;
            first_status = false;
        }

        os << "},\"tags\":{";

        bool first_tag = true;
        for (auto &[tag, count] : tag_counts) {
            if (!first_tag)
                os << ",";
            print_json_string(tag, os);
            os << ":" << count;
            first_tag = false;
        }

        os << "}}\n";
        return Mado_Error::OK;
    }

    // Mado_Output_Format::DEFAULT
    os << "Main directory: " << main_dir_path.string() << "\n";
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

    os << "Statuses:\n";
    if (!status_counts.empty()) {
        for (auto &[status, count] : status_counts)
            os << "  " << status << ": " << count << "\n";
    }

    os << "Tags:\n";
    if (!tag_counts.empty()) {
        for (auto &[tag, count] : tag_counts)
            os << "  " << tag << ": " << count << "\n";
    }

    return Mado_Error::OK;
}

std::pair<Mado_Output_Format, Mado_Error>
mado_parse_format(const std::string &format_str) {

    static const struct keyword_entry formats[] = {
        {"path", static_cast<int>(Mado_Output_Format::ONLY_PATH)},
        {"default", static_cast<int>(Mado_Output_Format::DEFAULT)},
        {"jsonl", static_cast<int>(Mado_Output_Format::JSONL)},
    };
    static const int n = sizeof(formats) / sizeof(formats[0]);

    // strip whitespace
    std::string cleaned(format_str);
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), is_whitespace), cleaned.end());

    int token = lookup_keyword(cleaned.c_str(), formats, n);
    if (token > 0) {
        return {static_cast<Mado_Output_Format>(token), Mado_Error::OK};
    }
    return {Mado_Output_Format::DEFAULT, Mado_Error::INVALID_FORMAT};
}

int mado_print_error(Mado_Error err, const char *context, std::ostream &os) {
    if (err != Mado_Error::OK) {
        os << "Mado error (" << context << "): " << mado_strerror(err) << "\n";
        return -1;
    }
    return 0;
}
