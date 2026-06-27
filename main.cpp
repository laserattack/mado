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
    std::cout << "Usage: " << cmd->usage << "\n\n";
    std::cout << cmd->description << "\n";
    if (cmd->options) {
        std::cout << "\nCommand options:\n";
        std::cout << options_help(cmd->options);
    }
}

static Command *find_by_name(const char *name, Command *commands) {
    for (int i = 0; commands[i].name; i++)
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
    return NULL;
}

} // namespace cmd

// ================ COMMAND TABLE

static int cmd_init(int argc, char **argv);
static int cmd_new(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_remove(int argc, char **argv);
static int cmd_info(int argc, char **argv);
static int cmd_help(int argc, char **argv);

static const cmd::Option LIST_OPTIONS[] = {
    {"sort", required_argument, NULL, 's', "Sort entries (+field,-field,field,...)"},
    {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"},
    {"abs-paths", no_argument, NULL, 'a', "Show absolute paths to entries"},
    {"only-hidden", no_argument, NULL, 'o', "Show only hidden fields"},
    {"hide-name", no_argument, NULL, 'n', "Hide name field"},
    {"hide-time", no_argument, NULL, 't', "Hide time field"},
    {"hide-deadline", no_argument, NULL, 'd', "Hide deadline field"},
    {"hide-priority", no_argument, NULL, 'p', "Hide priority field"},
    {"hide-status", no_argument, NULL, 'u', "Hide status field"},
    {"hide-tags", no_argument, NULL, 'g', "Hide tags field"},
    {"hide-path", no_argument, NULL, 'h', "Hide path field"},
    {NULL, 0, NULL, 0, NULL}};

static const cmd::Option REMOVE_OPTIONS[] = {
    {"abs-path", no_argument, NULL, 'a', "Show absolute path to removed entry"},
    {NULL, 0, NULL, 0, NULL}};

static const cmd::Option INFO_OPTIONS[] = {
    {"abs-path", no_argument, NULL, 'a', "Show absolute path to main directory"},
    {NULL, 0, NULL, 0, NULL}};

static cmd::Command commands[] = {
    {"init",
     "Initialize mado repository in current working directory",
     "mado init [COMMAND OPTIONS]",
     (cmd::Option[]){
         {"force", no_argument, NULL, 'F', "Force init"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_init},

    {"new",
     "Create new entry",
     "mado new [COMMAND OPTIONS]",
     (cmd::Option[]){
         {"template", required_argument, NULL, 't', "Template to use"},
         {"abs-path", no_argument, NULL, 'a', "Show absolute path to created entry"},
         {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_new},

    {"list",
     "List entries with optional filtering",
     "mado list [COMMAND OPTIONS] [QUERY]",
     LIST_OPTIONS,
     cmd_list},

    {"ls",
     "Alias for 'list' option",
     "mado ls [COMMAND OPTIONS] [QUERY]",
     LIST_OPTIONS,
     cmd_list},

    {"remove",
     "Remove entries matching query",
     "mado remove <QUERY>",
     REMOVE_OPTIONS,
     cmd_remove},

    {"rm",
     "Alias for 'remove' option",
     "mado rm <QUERY>",
     REMOVE_OPTIONS,
     cmd_remove},

    {"info",
     "Show repository information",
     "mado info",
     INFO_OPTIONS,
     cmd_info},

    {"repo",
     "Alias for 'info' option",
     "mado repo",
     INFO_OPTIONS,
     cmd_info},

    {"help",
     "Show help for commands",
     "mado help [COMMAND]",
     NULL,
     cmd_help},

    {NULL, NULL, NULL, NULL, NULL}};

static void print_usage() {
    std::cout << "Usage: " << argv0 << " [GLOBAL OPTIONS] [command] [COMMAND OPTIONS]\n\n";
    std::cout << "Global options:\n";
    std::cout << "  -C, --working-dir <DIR>   Change working directory\n";
    std::cout << "  -D, --main-dir <NAME>     Custom main directory name\n";
    std::cout << "  -E, --entry-file <NAME>   Custom entry file name\n";
    std::cout << "  -h, --help                Show this help\n";
    std::cout << "\nCommands:\n";
    for (int i = 0; commands[i].name; i++)
        fprintf(stdout, "  %-12s %s\n", commands[i].name, commands[i].description);
    std::cout << "\nRun '" << argv0 << " help <command>' for more information on a command\n";
}

// ================ COMMAND HANDLERS

static int cmd_init(int argc, char **argv) {
    int force = 0;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        if (opt == 'F')
            force = 1;
        else
            return -1;
    }
    Mado_Error err = mado_entries_dir_init(&g_mado_config, force);
    if (err == Mado_Error::FOUND_ABOVE) {
        mado_print_error(err, "creating entries directory");
        std::cerr << "Use -F to try force init here\n";
    }
    if (err == Mado_Error::ALREADY_EXISTS) {
        mado_print_error(err, "creating entries directory");
        std::cerr << "Already initialized in current directory\n";
    }
    if (err != Mado_Error::OK)
        mado_print_error(err, "creating entries directory");

    err = mado_templates_dir_init(&g_mado_config);
    if (err != Mado_Error::OK)
        mado_print_error(err, "creating templates directory");

    err = mado_hooks_dir_init(&g_mado_config);
    if (err != Mado_Error::OK)
        mado_print_error(err, "creating hooks directory");

    return 0;
}

static int cmd_new(int argc, char **argv) {
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        switch (opt) {
        case 't':
            g_mado_config.template_name = optarg;
            break;
        case 'a':
            g_mado_config.abs_paths = true;
            break;
        case 'f': {
            Mado_Error err = mado_parse_format(optarg, &g_mado_config.fmt);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing output format");
            break;
        }
        default:
            return -1;
        }
    }
    std::string main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (main_dir.empty()) {
        std::cerr << "Error: entries directory '" << g_mado_config.main_dir_name << "' not found\n";
        return -1;
    }

    // pre hook
    Mado_Error hook_err = mado_run_hook(&g_mado_config, "pre-new");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running pre-new hook");
        return -1;
    }

    auto [entry, err] = Mado_Entry::create(&g_mado_config, main_dir.c_str());
    if (err != Mado_Error::OK) {
        return mado_print_error(err, "creating new entry");
    }
    if (entry) {
        entry->print(&g_mado_config);

        // post hook
        hook_err = mado_run_hook(&g_mado_config, "post-new");
        if (hook_err != Mado_Error::OK) {
            mado_print_error(hook_err, "running post-new hook");
            return -1;
        }
    }

    return 0;
}

static int cmd_list(int argc, char **argv) {
    char *query = nullptr;
    Mado_Output_Format *fmt = &g_mado_config.fmt;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int only = 0, opt;

    opterr = 0;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1)
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

    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        switch (opt) {
        case 's': {
            Mado_Error err = mado_parse_sort(optarg, &g_mado_config.sort_criteria);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing sort option");
            break;
        }
        case 'f': {
            Mado_Error err = mado_parse_format(optarg, fmt);
            if (err != Mado_Error::OK)
                return mado_print_error(err, "parsing output format");
            break;
        }
        case 'a':
            g_mado_config.abs_paths = true;
            break;
        case 'o':
            break;
        case 'n':
            toggle_field(Mado_Entry_Field::NAME);
            break;
        case 't':
            toggle_field(Mado_Entry_Field::TIME);
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
            std::cerr << "Error: failed to parse query\n";
            return -1;
        }
    }

    std::string main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (main_dir.empty()) {
        std::cerr << "Error: entries directory '" << g_mado_config.main_dir_name << "' not found\n";
        return -1;
    }

    // pre hook
    Mado_Error hook_err = mado_run_hook(&g_mado_config, "pre-list");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running pre-list hook");
        return -1;
    }

    Mado_Entries::get_all(&g_mado_config, main_dir.c_str())
        .filter(filter.get())
        .sort(&g_mado_config)
        .print(&g_mado_config);

    // post hook
    hook_err = mado_run_hook(&g_mado_config, "post-list");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running post-list hook");
        return -1;
    }

    return 0;
}

static int cmd_remove(int argc, char **argv) {
    char *query = nullptr;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        switch (opt) {
        case 'a':
            g_mado_config.abs_paths = true;
            break;
        default:
            return -1;
        }
    }
    if (optind < argc)
        query = argv[optind];

    if (!query) {
        std::cerr << "Error: remove requires a query argument\n";
        return -1;
    }

    auto filter = make_ast_ptr(parse(query));
    if (!filter) {
        std::cerr << "Error: failed to parse query\n";
        return -1;
    }

    std::string main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (main_dir.empty()) {
        std::cerr << "Error: entries directory '" << g_mado_config.main_dir_name << "' not found\n";
        return -1;
    }

    // pre hook
    Mado_Error hook_err = mado_run_hook(&g_mado_config, "pre-remove");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running pre-remove hook");
        return -1;
    }

    auto removed = Mado_Entries::get_all(&g_mado_config, main_dir.c_str())
                       .filter(filter.get())
                       .remove();

    for (const auto &path : removed)
        std::cout << path << "\n";

    // post hook
    hook_err = mado_run_hook(&g_mado_config, "post-remove");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running post-remove hook");
        return -1;
    }

    return 0;
}

static int cmd_info([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        switch (opt) {
        case 'a':
            g_mado_config.abs_paths = true;
            break;
        default:
            return -1;
        }
    }

    // pre hook
    Mado_Error hook_err = mado_run_hook(&g_mado_config, "pre-info");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running pre-info hook");
        return -1;
    }

    Mado_Error err = mado_print_repo_info(&g_mado_config);
    if (err != Mado_Error::OK) {
        return mado_print_error(err, "getting repository info");
    }

    // post hook
    hook_err = mado_run_hook(&g_mado_config, "post-info");
    if (hook_err != Mado_Error::OK) {
        mado_print_error(hook_err, "running post-info hook");
        return -1;
    }

    return 0;
}

static int cmd_help(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }
    cmd::Command *cmd = cmd::find_by_name(argv[1], commands);
    if (!cmd) {
        std::cerr << "Unknown command: " << argv[1] << "\n\n";
        print_usage();
        return -1;
    }
    cmd::print_help(cmd);
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
    while ((opt = getopt_long(argc, argv, "+C:D:E:h", global_options, NULL)) != -1) {
        switch (opt) {
        case 'C':
            if (chdir(optarg) != 0) {
                std::cerr << "Error: failed to change directory to '" << optarg << "': " << strerror(errno) << "\n";
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
            print_usage();
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
        print_usage();
        return 0;
    }

    int ret = handle_global_options(argc, argv);
    if (ret != 0)
        return ret < 0 ? 1 : 0;

    if (optind >= argc) {
        print_usage();
        return 0;
    }

    char *command_name = argv[optind];
    cmd::Command *cmd = cmd::find_by_name(command_name, commands);
    if (!cmd) {
        std::cerr << "Error: unknown command '" << command_name << "'\n\n";
        print_usage();
        return 1;
    }

    int cmd_argc = argc - optind;
    char **cmd_argv = argv + optind;
    optind = 1;
    return cmd->handler(cmd_argc, cmd_argv) ? 1 : 0;
}
