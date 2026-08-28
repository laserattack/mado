#include <algorithm>
#include <cstring>
#include <ctime>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_set>
#include <vector>

#include "mado.hpp"

extern "C" {
#include "fuzzy_match.h"
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
    cfg->case_insensitive_search = false;
    cfg->parallel = false;
    cfg->load_all_first = false;
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

// ================ HELPERS

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

static Comparison_Field mado_field_to_ast_field(Mado_Entry_Field field) {
    switch (field) {
    case Mado_Entry_Field::PRIORITY:
        return CMP_PRIORITY;
    case Mado_Entry_Field::TAGS:
        return CMP_TAG;
    case Mado_Entry_Field::STATUS:
        return CMP_STATUS;
    case Mado_Entry_Field::PATH:
        return CMP_PATH;
    case Mado_Entry_Field::NAME:
        return CMP_NAME;
    case Mado_Entry_Field::TIME:
        return CMP_TIME;
    case Mado_Entry_Field::DEADLINE:
        return CMP_DEADLINE;
    case Mado_Entry_Field::MTIME:
        return CMP_MTIME;
    default:
        return CMP_ANY;
    }
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

    return {parse(cfg, entry_path, nullptr), Mado_Error::OK};
}

std::unique_ptr<Mado_Entry>
Mado_Entry::parse(const Mado_Config *cfg,
                  const std::filesystem::path &entry_dir,
                  const AST_Node *filter) {

    auto should_parse_field = [&](Mado_Entry_Field field) {
        bool hidden = has_flag(cfg->hide_fields, field);
        bool used_in_sorting = false;

        // If there is no filter, then all fields are parsed
        bool used_in_query = filter && ast_uses_field(filter, mado_field_to_ast_field(field));

        // used in sorting?
        for (const auto &criterion : cfg->sort_criteria) {
            if (criterion.field == field)
                used_in_sorting = true;
        }

        if (hidden && !used_in_sorting && !used_in_query)
            return false;

        return true;
    };

    auto is_valid_priority = [](const std::string &s) {
        return !s.empty() && s.size() <= 3 && std::all_of(s.begin(), s.end(), ::isdigit);
    };
    auto is_valid_deadline = [](const std::string &s) {
        if (s.empty() || s.size() > 15)
            return false;

        struct tm tm = {};
        char *end = nullptr;

        if (s.size() == 4)
            end = strptime(s.c_str(), "%Y", &tm);
        else if (s.size() == 6)
            end = strptime(s.c_str(), "%Y%m", &tm);
        else if (s.size() == 8)
            end = strptime(s.c_str(), "%Y%m%d", &tm);
        else if (s.size() >= 9 && s[8] == 'T') {
            std::string fmt = "%Y%m%dT";
            if (s.size() - 9 == 0) {
                // good (T without time part)
            } else if (s.size() - 9 == 2) {
                fmt += "%H";
            } else if (s.size() - 9 == 4) {
                fmt += "%H%M";
            } else if (s.size() - 9 == 6) {
                fmt += "%H%M%S";
            } else {
                return false;
            }
            end = strptime(s.c_str(), fmt.c_str(), &tm);
        } else {
            return false;
        }

        return end != nullptr && *end == '\0';
    };

    std::string dir_name = entry_dir.filename().string();

    auto entry_file = entry_dir / (cfg->entry_file_name + ".md");

    std::ifstream f(entry_file);
    if (!f)
        return nullptr;

    auto entry = std::make_unique<Mado_Entry>();
    entry->priority = 0;
    entry->time = dir_name;
    entry->path = entry_dir / (cfg->entry_file_name + ".md");
    entry->deadline = "99990101T000000";
    entry->mtime = "99990101T000000";

    bool need_name = should_parse_field(Mado_Entry_Field::NAME);
    bool need_priority = should_parse_field(Mado_Entry_Field::PRIORITY);
    bool need_tags = should_parse_field(Mado_Entry_Field::TAGS);
    bool need_status = should_parse_field(Mado_Entry_Field::STATUS);
    bool need_deadline = should_parse_field(Mado_Entry_Field::DEADLINE);
    bool need_mtime = should_parse_field(Mado_Entry_Field::MTIME);

    if (need_mtime) {

        // According to POSIX.1, localtime() is required to behave as though
        // tzset(3) was called, while localtime_r() does not have this
        // requirement.  For portable code, tzset(3) should be called before
        // localtime_r()
        static std::once_flag flag;
        std::call_once(flag, tzset);
        //

        struct stat st;
        if (stat(entry_file.c_str(), &st) == 0) {
            struct tm tm_result;
            localtime_r(&st.st_mtime, &tm_result);
            char buf[16];
            std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%S", &tm_result);
            entry->mtime = buf;
        } else {
            return nullptr;
        }
    }

    std::string line;
    int lines_processed = 0;

    // If true, it means the title has already been set
    bool has_field_name = false;
    bool has_field_priority = false;
    bool has_field_tags = false;
    bool has_field_status = false;
    bool has_field_deadline = false;

    while (lines_processed < cfg->max_header_lines && std::getline(f, line)) {
        lines_processed++;

        bool all_needed_found = true;
        if (need_name && !has_field_name)
            all_needed_found = false;
        if (need_priority && !has_field_priority)
            all_needed_found = false;
        if (need_tags && !has_field_tags)
            all_needed_found = false;
        if (need_status && !has_field_status)
            all_needed_found = false;
        if (need_deadline && !has_field_deadline)
            all_needed_found = false;
        if (all_needed_found)
            break;

        if (need_name &&
            !has_field_name &&
            line.rfind("- NAME:", 0) == 0) {

            entry->name = line.substr(7);
            trim(entry->name);
            has_field_name = true;

        } else if (need_priority &&
                   !has_field_priority &&
                   line.rfind("- PRIORITY:", 0) == 0) {

            std::string val = line.substr(11);
            trim(val);
            if (is_valid_priority(val))
                entry->priority = std::stoi(val);
            has_field_priority = true;

        } else if (need_tags &&
                   !has_field_tags &&
                   line.rfind("- TAGS:", 0) == 0) {

            std::string tags_str = line.substr(7);
            trim(tags_str);
            std::unordered_set<std::string> seen;
            std::stringstream ss(tags_str);
            std::string token;
            while (std::getline(ss, token, ',')) {
                trim(token);
                if (seen.insert(token).second) {
                    entry->tags.push_back(token);
                }
            }
            has_field_tags = true;

        } else if (need_status &&
                   !has_field_status &&
                   line.rfind("- STATUS:", 0) == 0) {

            entry->status = line.substr(9);
            trim(entry->status);
            has_field_status = true;

        } else if (need_deadline &&
                   !has_field_deadline &&
                   line.rfind("- DEADLINE:", 0) == 0) {

            std::string val = line.substr(11);
            trim(val);
            if (is_valid_deadline(val))
                entry->deadline = val;
            has_field_deadline = true;
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
        os << path.string() << "\n";
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

        if (has_flag(shown, Mado_Entry_Field::PATH)) {
            sep();
            os << "\"path\":\"" << path.string() << "\"";
        }
        if (has_flag(shown, Mado_Entry_Field::TIME)) {
            sep();
            os << "\"time\":";
            print_json_string(time, os);
        }
        if (has_flag(shown, Mado_Entry_Field::MTIME)) {
            sep();
            os << "\"mtime\":";
            print_json_string(mtime, os);
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
        os << "}\n";
        return;
    }

    // Mado_Output_Format::DEFAULT
    os << path.string() << ":1:";
    if (has_flag(shown, Mado_Entry_Field::PATH))
        os << " PATH:[" << path.string() << "]";
    if (has_flag(shown, Mado_Entry_Field::TIME))
        os << " TIME:[" << time << "]";
    if (has_flag(shown, Mado_Entry_Field::MTIME))
        os << " MTIME:[" << mtime << "]";
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

    auto check_string = [&](const std::string &v, const std::string &c) -> bool {
        const std::string vn = normalize_string_field(v);
        const std::string cn = normalize_string_field(c);

        switch (filter->comparison.cmp) {
        case CMP_EQ:
            return vn == cn;
        case CMP_NE:
            return vn != cn;
        case CMP_FUZZY:
            return fuzzy_match(cn.c_str(), vn.c_str(), cfg->case_insensitive_search) != INT32_MIN;
        case CMP_NFUZZY:
            return fuzzy_match(cn.c_str(), vn.c_str(), cfg->case_insensitive_search) == INT32_MIN;
        case CMP_SUBSTR:
            return vn.find(cn) != std::string::npos;
        case CMP_NSUBSTR:
            return vn.find(cn) == std::string::npos;
        case CMP_STARTS:
            return vn.rfind(cn, 0) == 0;
        case CMP_NSTARTS:
            return vn.rfind(cn, 0) != 0;
        case CMP_ENDS:
            return vn.size() >= cn.size() && vn.compare(vn.size() - cn.size(), cn.size(), cn) == 0;
        case CMP_NENDS:
            return !(vn.size() >= cn.size() && vn.compare(vn.size() - cn.size(), cn.size(), cn) == 0);
        case CMP_GT:
            return vn > cn;
        case CMP_LT:
            return vn < cn;
        case CMP_GE:
            return vn >= cn;
        case CMP_LE:
            return vn <= cn;
        default:
            return false;
        }
    };

    auto check_number = [&](uint16_t v, int c) -> bool {
        switch (filter->comparison.cmp) {
        case CMP_GT:
            return v > c;
        case CMP_LT:
            return v < c;
        case CMP_GE:
            return v >= c;
        case CMP_LE:
            return v <= c;
        case CMP_FUZZY:
        case CMP_SUBSTR:
        case CMP_ENDS:
        case CMP_STARTS:
        case CMP_EQ:
            return v == c;
        case CMP_NFUZZY:
        case CMP_NSUBSTR:
        case CMP_NSTARTS:
        case CMP_NENDS:
        case CMP_NE:
            return v != c;
        default:
            return false;
        }
    };

    switch (filter->type) {
    case NODE_ALL:
        return true;
    case NODE_UNTAGGED:
        // If there are no tags, the list contains one empty string by default
        return tags.size() == 1 && tags[0].empty();
    case NODE_UNSTATUSED:
        return status.empty();
    case NODE_UNNAMED:
        return name.empty();
    case NODE_UNPRIORITIZED:
        return priority == 0;
    case NODE_UNDEADLINED:
        return deadline == "99990101T000000";
    case NODE_BINARY_OP:
        switch (filter->binary.op) {
        case OP_AND:
            return matches_condition(cfg, filter->binary.left) && matches_condition(cfg, filter->binary.right);
        case OP_OR:
            return matches_condition(cfg, filter->binary.left) || matches_condition(cfg, filter->binary.right);
        case OP_XOR:
            return matches_condition(cfg, filter->binary.left) != matches_condition(cfg, filter->binary.right);
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
            const int c = filter->comparison.value.int_value;
            return check_number(priority, c);
        }
        case CMP_TAG: {
            const std::string c = filter->comparison.value.str_value;
            for (const auto &tag : tags) {
                if (check_string(tag, c))
                    return true;
            }
            return false;
        }
        case CMP_STATUS:
        case CMP_DEADLINE:
        case CMP_TIME:
        case CMP_MTIME:
        case CMP_PATH:
        case CMP_NAME: {
            const std::string c = filter->comparison.value.str_value;
            const std::string path_str = path.string();

            switch (filter->comparison.field) {
            case CMP_STATUS:
                return check_string(status, c);
            case CMP_DEADLINE:
                return check_string(deadline, c);
            case CMP_TIME:
                return check_string(time, c);
            case CMP_NAME:
                return check_string(name, c);
            case CMP_PATH:
                return check_string(path_str, c);
            case CMP_MTIME:
                return check_string(mtime, c);
            default:
                return false;
            }
        }
        case CMP_ANY: {
            const std::string c = filter->comparison.value.str_value;

            // number field
            if (!c.empty() && c.size() <= 3 && std::all_of(c.begin(), c.end(), ::isdigit)) {
                return check_number(priority, atoi(c.c_str()));
            }

            // string field
            if (check_string(name, c))
                return true;
            if (check_string(status, c))
                return true;
            if (check_string(path.string(), c))
                return true;
            if (check_string(time, c))
                return true;
            if (check_string(mtime, c))
                return true;
            if (check_string(deadline, c))
                return true;

            for (const auto &tag : tags) {
                if (check_string(tag, c))
                    return true;
            }

            return false;
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

static std::vector<std::filesystem::path>
collect_entry_dirs(const std::filesystem::path &main_dir) {
    std::vector<std::filesystem::path> dirs;

    auto is_entry_dir = [](const std::string &s) {
        return s.size() == 15 &&
               std::all_of(s.begin(), s.begin() + 8, ::isdigit) &&
               s[8] == 'T' &&
               std::all_of(s.begin() + 9, s.end(), ::isdigit);
    };

    if (!std::filesystem::exists(main_dir) || !std::filesystem::is_directory(main_dir))
        return dirs;

    for (const auto &dirent : std::filesystem::directory_iterator(main_dir)) {
        auto dir_path = dirent.path();
        std::string dir_name = dir_path.filename().string();

        if (is_entry_dir(dir_name))
            dirs.push_back(dir_path);
    }

    return dirs;
}

Mado_Entries
Mado_Entries::load_all(const Mado_Config *cfg,
                       const std::filesystem::path &main_dir,
                       const AST_Node *filter) {

    std::vector<std::filesystem::path> dirs = collect_entry_dirs(main_dir);
    Mado_Entries result;

    if (cfg->parallel) {
        std::vector<std::unique_ptr<Mado_Entry>> entries(dirs.size());

        std::for_each(std::execution::par, dirs.begin(), dirs.end(),
                      [&](const auto &dir) {
                          size_t idx = &dir - dirs.data();
                          entries[idx] = Mado_Entry::parse(cfg, dir, filter);
                      });

        for (auto &entry : entries) {
            if (entry) {
                result.entries_.push_back(std::move(entry));
            }
        }
    } else {
        for (const auto &dir : dirs) {
            auto entry = Mado_Entry::parse(cfg, dir, filter);
            if (entry) {
                result.entries_.push_back(std::move(entry));
            }
        }
    }

    return result;
}

Mado_Entries
Mado_Entries::load_matching(const Mado_Config *cfg,
                            const std::filesystem::path &main_dir,
                            const AST_Node *filter) {

    std::vector<std::filesystem::path> dirs = collect_entry_dirs(main_dir);
    Mado_Entries result;

    if (cfg->parallel) {
        std::mutex mtx;

        std::for_each(std::execution::par, dirs.begin(), dirs.end(),
                      [&](const auto &dir) {
                          auto entry = Mado_Entry::parse(cfg, dir, filter);
                          if (entry && entry->matches_condition(cfg, filter)) {
                              std::lock_guard<std::mutex> lock(mtx);
                              result.entries_.push_back(std::move(entry));
                          }
                      });
    } else {
        for (const auto &dir : dirs) {
            auto entry = Mado_Entry::parse(cfg, dir, filter);
            if (entry && entry->matches_condition(cfg, filter)) {
                result.entries_.push_back(std::move(entry));
            }
        }
    }

    return result;
}

Mado_Entries &Mado_Entries::filter(const Mado_Config *cfg, const AST_Node *filter) {
    if (!filter)
        return *this;

    if (cfg->parallel) {
        auto it = std::remove_if(std::execution::par, entries_.begin(), entries_.end(),
                                 [&](const std::unique_ptr<Mado_Entry> &entry) {
                                     return !entry->matches_condition(cfg, filter);
                                 });
        entries_.erase(it, entries_.end());
    } else {
        auto it = entries_.begin();
        while (it != entries_.end()) {
            if ((*it)->matches_condition(cfg, filter))
                ++it;
            else
                it = entries_.erase(it);
        }
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
            case Mado_Entry_Field::MTIME:
                compare_strings(a->mtime, b->mtime, less, greater);
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

    if (cfg->parallel) {
        std::sort(std::execution::par, entries_.begin(), entries_.end(), comparator);
    } else {
        std::stable_sort(entries_.begin(), entries_.end(), comparator);
    }

    return *this;
}

std::pair<std::vector<Mado_Sort_Criterion>, Mado_Error>
mado_parse_sort(const std::string &sort_str) {

    if (sort_str.empty())
        return {{}, Mado_Error::INVALID_FORMAT};

    std::stringstream ss(sort_str);
    std::string token;
    std::vector<Mado_Sort_Criterion> criteria;

    static const struct ident_entry fields_for_parse_sort[] = {
        {"time", static_cast<int>(Mado_Entry_Field::TIME)},
        {"mtime", static_cast<int>(Mado_Entry_Field::MTIME)},
        {"path", static_cast<int>(Mado_Entry_Field::PATH)},
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

        int field_token = lookup_ident(token.c_str(), fields_for_parse_sort, n_fields);
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
        std::filesystem::path dir_path = e->path.parent_path();
        std::filesystem::remove_all(dir_path);
        removed.push_back(dir_path.string());
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

    auto entries = Mado_Entries::load_all(cfg, main_dir_path, nullptr);

    struct EntryCounts {
        std::map<std::string, int> status_counts;
        std::map<std::string, int> tag_counts;
        std::map<uint16_t, int> priority_counts;
    };

    auto count_entries = [](const Mado_Entries &entries) {
        EntryCounts counts;
        for (const auto &e : entries) {
            counts.status_counts[e->status]++;

            for (const auto &tag : e->tags) {
                counts.tag_counts[tag]++;
            }

            counts.priority_counts[e->priority]++;
        }
        return counts;
    };

    auto counts = count_entries(entries);

    if (cfg->fmt == Mado_Output_Format::JSONL) {
        os << "{\"main_dir\":";
        print_json_string(main_dir_path.string(), os);
        os << ",\"entries_count\":" << entries.size()
           << ",\"statuses\":{";

        bool first_status = true;
        for (const auto &[status, count] : counts.status_counts) {
            if (!first_status)
                os << ",";
            print_json_string(status, os);
            os << ":" << count;
            first_status = false;
        }

        os << "},\"tags\":{";

        bool first_tag = true;
        for (const auto &[tag, count] : counts.tag_counts) {
            if (!first_tag)
                os << ",";
            print_json_string(tag, os);
            os << ":" << count;
            first_tag = false;
        }

        os << "},\"priorities\":{";

        bool first_priority = true;
        for (const auto &[priority, count] : counts.priority_counts) {
            if (!first_priority)
                os << ",";
            os << "\"" << priority << "\":" << count;
            first_priority = false;
        }

        os << "}}\n";
        return Mado_Error::OK;
    }

    // Mado_Output_Format::DEFAULT
    os << "Main directory: " << main_dir_path.string() << "\n";
    os << "Entries count: " << entries.size() << "\n";

    os << "Statuses:\n";
    if (!counts.status_counts.empty()) {
        for (const auto &[status, count] : counts.status_counts)
            os << "  " << status << ": " << count << "\n";
    }

    os << "Tags:\n";
    if (!counts.tag_counts.empty()) {
        for (const auto &[tag, count] : counts.tag_counts)
            os << "  " << tag << ": " << count << "\n";
    }

    os << "Priorities:\n";
    if (!counts.priority_counts.empty()) {
        for (const auto &[priority, count] : counts.priority_counts)
            os << "  " << priority << ": " << count << "\n";
    }

    return Mado_Error::OK;
}

std::pair<Mado_Output_Format, Mado_Error>
mado_parse_format(const std::string &format_str) {

    static const struct ident_entry formats[] = {
        {"path", static_cast<int>(Mado_Output_Format::ONLY_PATH)},
        {"default", static_cast<int>(Mado_Output_Format::DEFAULT)},
        {"jsonl", static_cast<int>(Mado_Output_Format::JSONL)},
    };
    static const int n = sizeof(formats) / sizeof(formats[0]);

    // strip whitespace
    std::string cleaned(format_str);
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), is_whitespace), cleaned.end());

    int token = lookup_ident(cleaned.c_str(), formats, n);
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
