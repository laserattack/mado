#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <getopt.h>
#include <unistd.h>

#include "mado.hpp"

#define UNUSED(x) (void)(x)

std::string find_dir_up(const std::string &dir_name) {
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

char *argv0;

// ================ GLOBAL CONFIG

static Mado_Config g_mado_config;

// ================ COMMAND SYSTEM

namespace cmd {

typedef int (*Handler)(int argc, char **argv);

struct My_Option {
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
    const My_Option *options;
    Handler handler;
};

static std::pair<std::vector<struct option>, std::string>
to_getopt(const My_Option *opts) {
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

static std::string options_help(const My_Option *opts) {
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
    fprintf(stdout, "Usage: %s\n\n", cmd->usage);
    fprintf(stdout, "%s\n", cmd->description);
    if (cmd->options) {
        fprintf(stdout, "\nCommand options:\n");
        std::string help = options_help(cmd->options);
        fprintf(stdout, "%s", help.c_str());
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

static cmd::Command commands[] = {
    {"init", "Initialize mado repository in current working directory", "mado init [COMMAND OPTIONS]",
     (cmd::My_Option[]){{"force", no_argument, NULL, 'F', "Force init"}, {NULL, 0, NULL, 0, NULL}}, cmd_init},
    {"new", "Create new entry", "mado new [COMMAND OPTIONS]",
     (cmd::My_Option[]){{"template", required_argument, NULL, 't', "Template to use (task, note)"}, {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"}, {NULL, 0, NULL, 0, NULL}}, cmd_new},
    {"list", "List entries with optional filtering", "mado list [COMMAND OPTIONS] [QUERY]",
     (cmd::My_Option[]){{"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"}, {"hide-name", no_argument, NULL, 'N', "Hide name field"}, {"hide-time", no_argument, NULL, 'T', "Hide time field"}, {"hide-deadline", no_argument, NULL, 'I', "Hide deadline field"}, {"hide-priority", no_argument, NULL, 'P', "Hide priority field"}, {"hide-status", no_argument, NULL, 'S', "Hide status field"}, {"hide-tags", no_argument, NULL, 'A', "Hide tags field"}, {"hide-path", no_argument, NULL, 'H', "Hide path field"}, {"only-hidden", no_argument, NULL, 'o', "Show only hidden fields"}, {NULL, 0, NULL, 0, NULL}}, cmd_list},
    {"remove", "Remove entries matching query", "mado remove <QUERY>", NULL, cmd_remove},
    {"info", "Show repository information", "mado info", NULL, cmd_info},
    {"help", "Show help for commands", "mado help [COMMAND]", NULL, cmd_help},
    {NULL, NULL, NULL, NULL, NULL}};

static void print_usage() {
    fprintf(stdout, "Usage: %s [GLOBAL OPTIONS] [command] [COMMAND OPTIONS]\n\n", argv0);
    fprintf(stdout, "Global options:\n");
    fprintf(stdout, "  -C, --working-dir <DIR>   Change working directory\n");
    fprintf(stdout, "  -D, --main-dir <NAME>     Custom main directory name\n");
    fprintf(stdout, "  -E, --entry-file <NAME>   Custom entry file name\n");
    fprintf(stdout, "  -h, --help                Show this help\n");
    fprintf(stdout, "\nCommands:\n");
    for (int i = 0; commands[i].name; i++)
        fprintf(stdout, "  %-12s %s\n", commands[i].name, commands[i].description);
    fprintf(stdout, "\nRun '%s help <command>' for more information on a command\n", argv0);
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
    if (mado_entries_dir_init(&g_mado_config, force) != 0)
        return -1;
    return mado_templates_dir_init(&g_mado_config);
}

static int cmd_new(int argc, char **argv) {
    Mado_Output_Format fmt = MADO_FMT_UNIX;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        switch (opt) {
        case 't':
            g_mado_config.template_name = optarg;
            break;
        case 'f':
            if (!mado_parse_format(optarg, &fmt)) {
                fprintf(stderr, "Error: unknown format '%s'\n", optarg);
                return -1;
            }
            break;
        default:
            return -1;
        }
    }
    std::string main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (main_dir.empty()) {
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_mado_config.main_dir_name.c_str());
        return -1;
    }
    return mado_entry_create_dir_and_md(&g_mado_config, main_dir.c_str(), fmt);
}

static int cmd_list(int argc, char **argv) {
    char *query = nullptr;
    Mado_Output_Format fmt = MADO_FMT_UNIX;
    const cmd::Command *cmd = cmd::find_by_name(argv[0], commands);
    auto [gopts, short_str] = cmd::to_getopt(cmd->options);
    int only = 0, opt;

    opterr = 0;
    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1)
        if (opt == 'o') {
            only = 1;
            g_mado_config.hide_fields = MADO_FIELD_ALL;
        }
    optind = 1;
    opterr = 1;

    auto toggle_field = [&](Mado_Entry_Field field) {
        if (only)
            g_mado_config.hide_fields = (Mado_Entry_Field)(g_mado_config.hide_fields & ~field);
        else
            g_mado_config.hide_fields = (Mado_Entry_Field)(g_mado_config.hide_fields | field);
    };

    while ((opt = getopt_long(argc, argv, short_str.c_str(), gopts.data(), NULL)) != -1) {
        switch (opt) {
        case 'f':
            if (!mado_parse_format(optarg, &fmt))
                return -1;
            break;
        case 'o':
            break;
        case 'N':
            toggle_field(MADO_FIELD_NAME);
            break;
        case 'T':
            toggle_field(MADO_FIELD_TIME);
            break;
        case 'I':
            toggle_field(MADO_FIELD_DEADLINE);
            break;
        case 'P':
            toggle_field(MADO_FIELD_PRIORITY);
            break;
        case 'S':
            toggle_field(MADO_FIELD_STATUS);
            break;
        case 'A':
            toggle_field(MADO_FIELD_TAGS);
            break;
        case 'H':
            toggle_field(MADO_FIELD_PATH);
            break;
        default:
            return -1;
        }
    }
    if (optind < argc)
        query = argv[optind];

    ASTNode *filter = nullptr;
    if (query) {
        filter = parse(query);
        if (!filter) {
            fprintf(stderr, "Error: failed to parse query\n");
            return -1;
        }
    }

    std::string main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (main_dir.empty()) {
        ast_free(filter);
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_mado_config.main_dir_name.c_str());
        return -1;
    }

    Mado_Entries::get_all(&g_mado_config, main_dir.c_str())
        .filter(filter)
        .print(&g_mado_config, fmt);

    ast_free(filter);
    return 0;
}

static int cmd_remove(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: remove requires a query argument\n");
        return -1;
    }

    ASTNode *filter = parse(argv[1]);
    if (!filter) {
        fprintf(stderr, "Error: failed to parse query\n");
        return -1;
    }

    std::string main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (main_dir.empty()) {
        ast_free(filter);
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_mado_config.main_dir_name.c_str());
        return -1;
    }

    Mado_Entries::get_all(&g_mado_config, main_dir.c_str())
        .filter(filter)
        .remove();

    ast_free(filter);
    return 0;
}

static int cmd_info(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    return mado_print_repo_info(&g_mado_config);
}

static int cmd_help(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }
    cmd::Command *cmd = cmd::find_by_name(argv[1], commands);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
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
                fprintf(stderr, "Error: failed to change directory to '%s': %s\n", optarg, strerror(errno));
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
    if (mado_init(&g_mado_config) != 0)
        return 1;
    atexit(mado_deinit);

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
        fprintf(stderr, "Error: unknown command '%s'\n\n", command_name);
        print_usage();
        return 1;
    }

    int cmd_argc = argc - optind;
    char **cmd_argv = argv + optind;
    optind = 1;
    return cmd->handler(cmd_argc, cmd_argv) ? 1 : 0;
}
