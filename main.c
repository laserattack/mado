#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/da.h"
#include "utils/fs.h"
#include "utils/util.h"

#include "ast.h"
#include "mado.h"

#define UNUSED(x) (void)(x)

char *argv0;

// ================ GLOBAL CONFIG

static Mado_Config g_mado_config;

// ================ COMMAND SYSTEM

typedef struct {
    const char *name;
    int has_arg;
    int *flag;
    int val;
    const char *description;
} Option;

typedef int (*command_handler)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *description;
    const char *usage;
    const Option *options;
    command_handler handler;
} Command;

static int cmd_init(int argc, char **argv);
static int cmd_new(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_remove(int argc, char **argv);
static int cmd_info(int argc, char **argv);
static int cmd_help(int argc, char **argv);

static Command commands[] = {
    {"init", "Initialize mado repository in current working directory", "mado init [COMMAND OPTIONS]",
     (Option[]){
         {"force", no_argument, NULL, 'F', "Force init"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_init},
    {"new", "Create new entry", "mado new [COMMAND OPTIONS]",
     (Option[]){
         {"template", required_argument, NULL, 't', "Template to use (task, note)"},
         {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_new},
    {"list", "List entries with optional filtering", "mado list [COMMAND OPTIONS] [QUERY]",
     (Option[]){
         {"format", required_argument, NULL, 'f', "Output format (unix, path, jsonl)"},
         {"hide-name", no_argument, NULL, 'N', "Hide name field"},
         {"hide-time", no_argument, NULL, 'T', "Hide time field"},
         {"hide-deadline", no_argument, NULL, 'I', "Hide deadline field"},
         {"hide-priority", no_argument, NULL, 'P', "Hide priority field"},
         {"hide-status", no_argument, NULL, 'S', "Hide status field"},
         {"hide-tags", no_argument, NULL, 'A', "Hide tags field"},
         {"hide-path", no_argument, NULL, 'H', "Hide path field"},
         {"only-hidden", no_argument, NULL, 'o', "Show only hidden fields"},
         {NULL, 0, NULL, 0, NULL}},
     cmd_list},
    {"remove", "Remove entries matching query", "mado remove <QUERY>", NULL, cmd_remove},
    {"info", "Show repository information", "mado info", NULL, cmd_info},
    {"help", "Show help for commands", "mado help [COMMAND]", NULL, cmd_help},
    {NULL, NULL, NULL, NULL, NULL}};

static struct option *option_to_getopt(const Option *opts, char **short_str) {
    int n = 0;
    while (opts[n].name)
        n++;
    struct option *gopts = malloc((n + 1) * sizeof(struct option));
    size_t ssize = 32;
    char *sstr = malloc(ssize);
    sstr[0] = '\0';

    for (int i = 0; i < n; i++) {
        gopts[i].name = opts[i].name;
        gopts[i].has_arg = opts[i].has_arg;
        gopts[i].flag = opts[i].flag;
        gopts[i].val = opts[i].val;

        if (opts[i].val && isprint(opts[i].val)) {
            size_t len = strlen(sstr);
            if (len + 3 >= ssize) {
                ssize *= 2;
                sstr = realloc(sstr, ssize);
            }
            sstr[len] = opts[i].val;
            if (opts[i].has_arg == required_argument) {
                sstr[len + 1] = ':';
                sstr[len + 2] = '\0';
            } else if (opts[i].has_arg == optional_argument) {
                sstr[len + 1] = ':';
                sstr[len + 2] = ':';
                sstr[len + 3] = '\0';
            } else {
                sstr[len + 1] = '\0';
            }
        }
    }
    gopts[n] = (struct option){0, 0, 0, 0};
    *short_str = sstr;
    return gopts;
}

static char *options_help(const Option *opts) {
    size_t size = 256;
    size_t used = 0;
    char *buf = malloc(size);
    buf[0] = '\0';

    for (int i = 0; opts[i].name; i++) {
        const char *fmt = opts[i].val && isprint(opts[i].val)
                              ? "  -%c, --%-16s %s\n"
                              : "  --%-20s %s\n";

        int needed = snprintf(NULL, 0, fmt,
                              opts[i].val, opts[i].name, opts[i].description);

        while (used + needed + 1 > size) {
            size *= 2;
            buf = realloc(buf, size);
        }

        if (opts[i].val && isprint(opts[i].val))
            used += sprintf(buf + used, fmt, opts[i].val, opts[i].name, opts[i].description);
        else
            used += sprintf(buf + used, fmt, opts[i].name, opts[i].description);
    }
    return buf;
}

static void print_command_help(const Command *cmd) {
    fprintf(stdout, "Usage: %s\n\n", cmd->usage);
    fprintf(stdout, "%s\n", cmd->description);
    if (cmd->options) {
        fprintf(stdout, "\nCommand options:\n");
        char *help = options_help(cmd->options);
        fprintf(stdout, "%s", help);
        free(help);
    }
}

static Command *find_command(const char *name) {
    for (int i = 0; commands[i].name; i++)
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
    return NULL;
}

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
    const Command *cmd = find_command(argv[0]);
    char *short_str;
    struct option *gopts = option_to_getopt(cmd->options, &short_str);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        switch (opt) {
        case 'F':
            force = 1;
            break;
        default:
            free(short_str);
            free(gopts);
            return -1;
        }
    }
    free(short_str);
    free(gopts);
    if (mado_entries_dir_init(&g_mado_config, force) != 0)
        return -1;
    return mado_templates_dir_init(&g_mado_config);
}

static int cmd_new(int argc, char **argv) {
    Mado_Output_Format fmt = MADO_FMT_UNIX;
    const Command *cmd = find_command(argv[0]);
    char *short_str;
    struct option *gopts = option_to_getopt(cmd->options, &short_str);
    int opt;
    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        switch (opt) {
        case 't':
            g_mado_config.template_name = optarg;
            break;
        case 'f':
            if (!mado_parse_format(optarg, &fmt)) {
                fprintf(stderr, "Error: unknown format '%s'\n", optarg);
                free(short_str);
                free(gopts);
                return -1;
            }
            break;
        default:
            free(short_str);
            free(gopts);
            return -1;
        }
    }
    free(short_str);
    free(gopts);
    char *main_dir = find_dir_up(g_mado_config.main_dir_name);
    if (!main_dir) {
        fprintf(stderr, "Error: entries directory '%s' not found\n", g_mado_config.main_dir_name);
        return -1;
    }
    int ret = mado_entry_create_dir_and_md(&g_mado_config, main_dir, fmt);
    free(main_dir);
    return ret;
}

static int cmd_list(int argc, char **argv) {
    char *query = NULL;
    Mado_Output_Format fmt = MADO_FMT_UNIX;
    const Command *cmd = find_command(argv[0]);
    char *short_str;
    struct option *gopts = option_to_getopt(cmd->options, &short_str);
    int only = 0, opt;

    opterr = 0;
    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1)
        if (opt == 'o') {
            only = 1;
            g_mado_config.hide_fields = MADO_FIELD_ALL;
        }
    optind = 1;
    opterr = 1;

    while ((opt = getopt_long(argc, argv, short_str, gopts, NULL)) != -1) {
        switch (opt) {
        case 'f':
            if (!mado_parse_format(optarg, &fmt)) {
                free(short_str);
                free(gopts);
                return -1;
            }
            break;
        case 'o':
            break;
        case 'N':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_NAME) : (g_mado_config.hide_fields | MADO_FIELD_NAME);
            break;
        case 'T':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_TIME) : (g_mado_config.hide_fields | MADO_FIELD_TIME);
            break;
        case 'I':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_DEADLINE) : (g_mado_config.hide_fields | MADO_FIELD_DEADLINE);
            break;
        case 'P':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_PRIORITY) : (g_mado_config.hide_fields | MADO_FIELD_PRIORITY);
            break;
        case 'S':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_STATUS) : (g_mado_config.hide_fields | MADO_FIELD_STATUS);
            break;
        case 'A':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_TAGS) : (g_mado_config.hide_fields | MADO_FIELD_TAGS);
            break;
        case 'H':
            g_mado_config.hide_fields = only ? (g_mado_config.hide_fields & ~MADO_FIELD_PATH) : (g_mado_config.hide_fields | MADO_FIELD_PATH);
            break;
        default:
            free(short_str);
            free(gopts);
            return -1;
        }
    }
    free(short_str);
    free(gopts);
    if (optind < argc)
        query = argv[optind];

    struct {
        const Mado_Config *cfg;
        Mado_Output_Format fmt;
    } ctx = {&g_mado_config, fmt};
    return mado_entries_process(&g_mado_config, query, mado_entry_op_print, &ctx);
}

static int cmd_remove(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: remove requires a query argument\n");
        return -1;
    }
    return mado_entries_process(&g_mado_config, argv[1], mado_entry_op_delete, NULL);
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
    Command *cmd = find_command(argv[1]);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        print_usage();
        return -1;
    }
    print_command_help(cmd);
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
    Command *cmd = find_command(command_name);
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
