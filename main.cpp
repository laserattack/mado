#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <getopt.h>
#include <unistd.h>

#include "mado.hpp"

static char *argv0;

// ================ GLOBAL CONFIG

static Mado_Config g_mado_config;

// ================ COMMAND SYSTEM

namespace cmd {

typedef int (*Handler)(int argc, char **argv);

struct Option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
    const char *description;
};

struct Command {
    const char *name;
    const char *description;
    const char *usage;
    const Option *options;
    Handler handler;
    const char **aliases;
};

static std::pair<std::vector<struct option>, std::string>
to_getopt(const Option *opts) {
    int n = 0;
    while (opts[n].name)
        n++;
    std::vector<struct option> gopts(n + 1);
    std::string short_str;
    for (int i = 0; i < n; i++) {
        gopts[i].name = opts[i].name;
        gopts[i].has_arg = opts[i].has_arg;
        gopts[i].flag = opts[i].flag;
        gopts[i].val = opts[i].val;
        if (opts[i].val && isprint(opts[i].val)) {
            short_str += opts[i].val;
            if (opts[i].has_arg == required_argument)
                short_str += ':';
            else if (opts[i].has_arg == optional_argument)
                short_str += "::";
        }
    }
    gopts[n] = {nullptr, 0, nullptr, 0};
    return {gopts, short_str};
}

static std::string options_help(const Option *opts) {
    std::string result;
    for (int i = 0; opts[i].name; i++) {
        if (opts[i].val && isprint(opts[i].val)) {
            result += "  -" + std::string(1, opts[i].val) + ", --";
            result += opts[i].name;
            int padding = 16 - strlen(opts[i].name);
            if (padding > 0)
                result.append(padding, ' ');
            result += " " + std::string(opts[i].description) + "\n";
        } else {
            result += "  --";
            result += opts[i].name;
            int padding = 20 - strlen(opts[i].name);
            if (padding > 0)
                result.append(padding, ' ');
            result += " " + std::string(opts[i].description) + "\n";
        }
    }
    return result;
}

static void print_help(const Command *cmd) {
    std::cerr << "Usage: " << cmd->usage << "\n\n";
    std::cerr << cmd->description << "\n";
    if (cmd->options) {
        std::cerr << "\nCommand options:\n";
        std::cerr << options_help(cmd->options);
    }
}

static Command *find_by_name(const char *name, Command *commands) {
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
        if (commands[i].aliases) {
            for (int j = 0; commands[i].aliases[j]; j++) {
                if (strcmp(commands[i].aliases[j], name) == 0)
                    return &commands[i];
            }
        }
    }
    return nullptr;
}

} // namespace cmd

// ================ COMMAND TABLE

static int cmd_init(int argc, char **argv);
static int cmd_new(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_remove(int argc, char **argv);
static int cmd_info(int argc, char **argv);
static int cmd_help(int argc, char **argv);

static const cmd::Option COMMAND_LIST_OPTIONS[] = {
    {"sort", required_argument, nullptr, 's', "Sort entries (+field,-field,field,...)"},
    {"format", required_argument, nullptr, 'f', "Output format (default, path, jsonl)"},
    {"ignore-case", no_argument, nullptr, 'i', "Case-insensitive search"},
    {"only-hidden", no_argument, nullptr, 'o', "Show only hidden fields"},
    {"hide-name", no_argument, nullptr, 'n', "Hide name field"},
    {"hide-time", no_argument, nullptr, 't', "Hide time field"},
    {"hide-mtime", no_argument, nullptr, 'm', "Hide mtime field"},
    {"hide-deadline", no_argument, nullptr, 'd', "Hide deadline field"},
    {"hide-priority", no_argument, nullptr, 'p', "Hide priority field"},
    {"hide-status", no_argument, nullptr, 'u', "Hide status field"},
    {"hide-tags", no_argument, nullptr, 'g', "Hide tags field"},
    {"hide-path", no_argument, nullptr, 'h', "Hide path field"},
    {nullptr, 0, nullptr, 0, nullptr}};

static const cmd::Option COMMAND_REMOVE_OPTIONS[] = {
    {"ignore-case", no_argument, nullptr, 'i', "Case-insensitive search"},
    {nullptr, 0, nullptr, 0, nullptr}};

static const cmd::Option COMMAND_INFO_OPTIONS[] = {
    {"format", required_argument, nullptr, 'f', "Output format (default, jsonl)"},
    {"dir", no_argument, nullptr, 'd', "Print main directory path only"},
    {nullptr, 0, nullptr, 0, nullptr}};

static const cmd::Option COMMAND_NEW_OPTIONS[] = {
    {"template", required_argument, nullptr, 't', "Template to use"},
    {"format", required_argument, nullptr, 'f', "Output format (default, path, jsonl)"},
    {nullptr, 0, nullptr, 0, nullptr}};

static const cmd::Option COMMAND_INIT_OPTIONS[] = {
    {"force", no_argument, nullptr, 'F', "Force init"},
    {nullptr, 0, nullptr, 0, nullptr}};

static const cmd::Option COMMAND_HELP_OPTIONS[] = {
    {"show-aliases", no_argument, nullptr, 'a', "Show command aliases"},
    {nullptr, 0, nullptr, 0, nullptr}};

static cmd::Command commands[] = {

    {
        "init",
        "Initialize mado repository in current working directory",
        "mado init [COMMAND OPTIONS]",
        COMMAND_INIT_OPTIONS,
        cmd_init,
        (const char *[]){"setup", "start", "install", nullptr},
    },

    {
        "new",
        "Create new entry",
        "mado new [COMMAND OPTIONS]",
        COMMAND_NEW_OPTIONS,
        cmd_new,
        (const char *[]){"add", "create", "note", "task", nullptr},
    },

    {
        "list",
        "List entries with optional filtering",
        "mado list [COMMAND OPTIONS] [QUERY]",
        COMMAND_LIST_OPTIONS,
        cmd_list,
        (const char *[]){"ls", "show", "search", "find", "view", nullptr},
    },

    {
        "remove",
        "Remove entries matching query",
        "mado remove [COMMAND OPTIONS] <QUERY>",
        COMMAND_REMOVE_OPTIONS,
        cmd_remove,
        (const char *[]){"rm", "delete", "clear", "trash", nullptr},
    },

    {
        "info",
        "Show repository information",
        "mado info [COMMAND OPTIONS]",
        COMMAND_INFO_OPTIONS,
        cmd_info,
        (const char *[]){"repo", "about", "summary", "stat", nullptr},
    },

    {
        "help",
        "Show help for commands",
        "mado help [COMMAND OPTIONS] [COMMAND]",
        COMMAND_HELP_OPTIONS,
        cmd_help,
        (const char *[]){"usage", nullptr},
    },

    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}};

static int count_aliases(const cmd::Command *cmd) {
    if (!cmd->aliases)
        return 0;
    int count = 0;
    while (cmd->aliases[count])
        count++;
    return count;
}

static void print_usage(bool show_aliases) {
    std::cerr << "Usage: " << argv0 << " [GLOBAL OPTIONS] [command] [COMMAND OPTIONS]\n\n";
    std::cerr << "Global options:\n";
    std::cerr << "  -C, --working-dir <DIR>   Change working directory\n";
    std::cerr << "  -D, --main-dir <NAME>     Custom main directory name\n";
    std::cerr << "  -E, --entry-file <NAME>   Custom entry file name\n";
    std::cerr << "  -h, --help                Show this help\n";
    std::cerr << "\nCommands:\n";
    for (int i = 0; commands[i].name; i++) {
        fprintf(stderr, "  %-12s %s\n", commands[i].name, commands[i].description);
        if (show_aliases && commands[i].aliases && commands[i].aliases[0]) {
            for (int j = 0; commands[i].aliases[j]; j++) {
                fprintf(stderr, "  %-12s %s (alias for '%s')\n",
                        commands[i].aliases[j],
                        commands[i].description,
                        commands[i].name);
            }
        }
    }
    std::cerr << "\nRun '" << argv0 << " help <command>' for more information on a command\n";
}

// ================ COMMAND HANDLERS

static int cmd_init(int argc, char **argv) {
    int force = 0;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1) {
        switch (opt) {
        case 'F':
            force = 1;
            break;
        default:
            return -1;
        }
    }

    auto [main_dir, err] = mado_main_dir_init(&g_mado_config, force);

    auto print_main_dir_path = [&]() {
        std::cout << main_dir.string() << "\n";
    };

    if (err == Mado_Error::FOUND_ABOVE) {
        mado_print_error(err, "creating main directory");
        std::cerr << "Hint: Use -F to try force init here\n";
        print_main_dir_path();
    } else if (err == Mado_Error::ALREADY_EXISTS) {
        mado_print_error(err, "creating main directory");
        std::cerr << "Hint: Already initialized in current directory\n";
        print_main_dir_path();
    } else if (err != Mado_Error::OK) {
        mado_print_error(err, "creating main directory");
    } else { // err == Mado_Error::OK
        print_main_dir_path();
    }

    err = mado_templates_dir_init(&g_mado_config);
    if (err != Mado_Error::OK)
        mado_print_error(err, "creating templates directory");

    return 0;
}

static int cmd_new(int argc, char **argv) {
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1) {
        switch (opt) {
        case 't':
            g_mado_config.template_name = optarg;
            break;
        case 'f': {
            auto [fmt, err] = mado_parse_format(optarg);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing output format");

            g_mado_config.fmt = fmt;
            break;
        }
        default:
            return -1;
        }
    }
    auto [main_dir, find_main_dir_err] = mado_find_main_dir(&g_mado_config);
    if (find_main_dir_err != Mado_Error::OK) {
        return mado_print_error(find_main_dir_err, "finding main directory");
    }

    auto [entry, err] = Mado_Entry::create(&g_mado_config, main_dir);
    if (err != Mado_Error::OK) {
        return mado_print_error(err, "creating new entry");
    }
    if (entry) {
        entry->print(&g_mado_config);
    }

    return 0;
}

static int cmd_list(int argc, char **argv) {
    char *query = nullptr;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int only = 0, opt;

    opterr = 0;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1)
        if (opt == 'o') {
            only = 1;
            g_mado_config.hide_fields = Mado_Entry_Field::ALL;
        }
    optind = 1;
    opterr = 1;

    auto toggle_field = [&](Mado_Entry_Field field) {
        if (only)
            g_mado_config.hide_fields &= ~field;
        else
            g_mado_config.hide_fields |= field;
    };

    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1) {
        switch (opt) {
        case 's': {
            auto [criteria, err] = mado_parse_sort(optarg);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing sort option");

            g_mado_config.sort_criteria = criteria;
            break;
        }
        case 'f': {
            auto [fmt, err] = mado_parse_format(optarg);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing output format");

            g_mado_config.fmt = fmt;
            break;
        }
        case 'i':
            g_mado_config.case_insensitive_search = true;
            break;
        case 'o':
            break;
        case 'n':
            toggle_field(Mado_Entry_Field::NAME);
            break;
        case 't':
            toggle_field(Mado_Entry_Field::TIME);
            break;
        case 'm':
            toggle_field(Mado_Entry_Field::MTIME);
            break;
        case 'd':
            toggle_field(Mado_Entry_Field::DEADLINE);
            break;
        case 'p':
            toggle_field(Mado_Entry_Field::PRIORITY);
            break;
        case 'u':
            toggle_field(Mado_Entry_Field::STATUS);
            break;
        case 'g':
            toggle_field(Mado_Entry_Field::TAGS);
            break;
        case 'h':
            toggle_field(Mado_Entry_Field::PATH);
            break;
        default:
            return -1;
        }
    }

    if (optind < argc)
        query = argv[optind];

    auto filter = make_ast_ptr(nullptr);
    if (query) {
        filter = make_ast_ptr(parse(query));
        if (!filter) {
            return mado_print_error(Mado_Error::PARSE, "parsing query");
        }
    }

    auto [main_dir, err] = mado_find_main_dir(&g_mado_config);
    if (err != Mado_Error::OK) {
        return mado_print_error(err, "finding main directory");
    }

    Mado_Entries::get_all(&g_mado_config, main_dir)
        .filter(&g_mado_config, filter.get())
        .sort(&g_mado_config)
        .print(&g_mado_config);

    return 0;
}

static int cmd_remove(int argc, char **argv) {
    char *query = nullptr;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1) {
        switch (opt) {
        case 'i':
            g_mado_config.case_insensitive_search = true;
            break;
        default:
            return -1;
        }
    }
    if (optind < argc)
        query = argv[optind];

    if (!query) {
        return mado_print_error(Mado_Error::PARSE, "remove requires a query argument");
    }

    auto filter = make_ast_ptr(parse(query));
    if (!filter) {
        return mado_print_error(Mado_Error::PARSE, "parsing query");
    }

    auto [main_dir, err] = mado_find_main_dir(&g_mado_config);
    if (err != Mado_Error::OK) {
        return mado_print_error(err, "finding main directory");
    }

    auto removed = Mado_Entries::get_all(&g_mado_config, main_dir)
                       .filter(&g_mado_config, filter.get())
                       .remove();

    for (const auto &path : removed)
        std::cout << path << "\n";

    return 0;
}

static int cmd_info([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    bool print_main_dir_path_only = false;

    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1) {
        switch (opt) {
        case 'f': {

            auto [fmt, err] = mado_parse_format(optarg);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing output format");

            // invalid formats for this command
            if (fmt == Mado_Output_Format::ONLY_PATH) {
                err = Mado_Error::INVALID_FORMAT;
                return mado_print_error(err, "parsing output format");
            }

            g_mado_config.fmt = fmt;
            break;
        }
        case 'd':
            print_main_dir_path_only = true;
            break;
        default:
            return -1;
        }
    }

    if (print_main_dir_path_only) {
        auto [main_dir_path, err] = mado_find_main_dir(&g_mado_config);
        if (err != Mado_Error::OK) {
            return mado_print_error(err, "finding main directory");
        }

        std::cout << main_dir_path.string() << std::endl;
        return 0;
    }

    Mado_Error err = mado_print_repo_info(&g_mado_config);
    if (err == Mado_Error::NOT_FOUND) {
        return mado_print_error(err, "finding main directory");
    } else if (err != Mado_Error::OK) {
        return mado_print_error(err, "getting repository info");
    }

    return 0;
}

static int cmd_help(int argc, char **argv) {
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    bool show_aliases = false;
    int opt;

    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), nullptr)) != -1) {
        switch (opt) {
        case 'a':
            show_aliases = true;
            break;
        default:
            return -1;
        }
    }

    if (optind < argc) {
        const char *target_cmd_name = argv[optind];
        cmd::Command *target_cmd = cmd::find_by_name(target_cmd_name, commands);

        if (!target_cmd) {
            std::cerr << "Hint: Unknown command: " << target_cmd_name << "\n\n";
            print_usage(show_aliases);
            return -1;
        }

        bool is_alias = strcmp(target_cmd->name, target_cmd_name) != 0;

        if (is_alias) {
            std::cerr << "Hint: '" << target_cmd_name << "' is an alias for '" << target_cmd->name << "'\n\n";
        }

        cmd::print_help(target_cmd);

        if (show_aliases && target_cmd->aliases && target_cmd->aliases[0]) {

            auto print_aliases = [&]() {
                bool first = true;
                for (int i = 0; target_cmd->aliases[i]; i++) {
                    if (strcmp(target_cmd->aliases[i], target_cmd_name) == 0)
                        continue;
                    if (!first)
                        std::cerr << ", ";
                    std::cerr << target_cmd->aliases[i];
                    first = false;
                }
                std::cerr << "\n";
            };

            if (is_alias) {
                if (count_aliases(target_cmd) > 1) {
                    std::cerr << "\nOther aliases for '" << target_cmd->name << "': ";
                    print_aliases();
                }
            } else {
                std::cerr << "\nAliases: ";
                print_aliases();
            }
        }

    } else {
        print_usage(show_aliases);
    }

    return 0;
}

// ================ GLOBAL OPTIONS

static int handle_global_options(int argc, char **argv) {
    static struct option global_options[] = {
        {"working-dir", required_argument, 0, 'C'},
        {"main-dir", required_argument, 0, 'D'},
        {"entry-file", required_argument, 0, 'E'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};
    int opt;
    while ((opt = getopt_long(argc, argv, "+C:D:E:h", global_options, nullptr)) != -1) {
        switch (opt) {
        case 'C':
            if (chdir(optarg) != 0) {
                mado_print_error(Mado_Error::IO, "changing working directory");
                std::cerr << "Hint: " << strerror(errno) << "\n";
                return -1;
            }
            break;
        case 'D':
            g_mado_config.main_dir_name = optarg;
            break;
        case 'E':
            g_mado_config.entry_file_name = optarg;
            break;
        case 'h':
            print_usage(false);
            return 1;
        default:
            return -1;
        }
    }
    return 0;
}

// ================ ENTRY POINT

int main(int argc, char **argv) {
    argv0 = argv[0];
    mado_init_config(&g_mado_config);

    if (argc < 2) {
        print_usage(false);
        return 0;
    }

    int ret = handle_global_options(argc, argv);
    if (ret != 0)
        return ret < 0 ? 1 : 0;

    if (optind >= argc) {
        print_usage(false);
        return 0;
    }

    char *command_name = argv[optind];
    cmd::Command *cmd = cmd::find_by_name(command_name, commands);
    if (!cmd) {
        mado_print_error(Mado_Error::NOT_FOUND, "unknown command");
        std::cerr << "Hint: '" << command_name << "' is not a valid mado command\n\n";
        print_usage(false);
        return 1;
    }

    int cmd_argc = argc - optind;
    char **cmd_argv = argv + optind;
    optind = 1;
    return cmd->handler(cmd_argc, cmd_argv) ? 1 : 0;
}
