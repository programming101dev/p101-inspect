#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_cli/p101_getopt.h>
#include <p101_cli/p101_stdlib.h>
#include <p101_cli/p101_unistd.h>
#include <stdlib.h>

void p101_observe_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
}

void p101_observe_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE_SCOPE(env);
    opterr = 0;

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        args->show_help = true;
        return;
    }

    while((opt = p101_getopt(env, argc, argv, ":hvARo:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)    // GCOVR_EXCL_BR_LINE -- default is a defensive p101_getopt contract check
        {
            case 'h':
            {
                args->show_help = true;
                break;
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'A':
            {
                args->log_arguments = true;
                break;
            }
            case 'R':
            {
                args->log_results = true;
                break;
            }
            case 'o':
            {
                args->report_dir = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');    // GCOVR_EXCL_BR_LINE -- getopt supplies optopt for this case
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(p101_isprint(env, optopt))
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:    // GCOVR_EXCL_START
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c' returned by getopt.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
                // GCOVR_EXCL_STOP
        }
    }

    if(p101_error_has_no_error(err))
    {
        args->command_argv = &argv[optind];
    }
}

void p101_observe_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE_SCOPE(env);

    if(args->command_argv == NULL || args->command_argv[0] == NULL)
    {
        P101_ERROR_RAISE_USER(err, "A command is required.", ERR_USAGE);
        goto done;
    }

    if(args->report_dir != NULL && args->report_dir[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The report directory must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

void p101_observe_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE_SCOPE(env);
    (void)exit_code;

#ifndef P101_SUPPRESS_USAGE_TEXT
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-A] [-R] [-o <capture-dir>] -- <command> [args...]\n", program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                       Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                       Enable verbose p101 tracing in p101-observe\n", stderr);
    p101_fputs(env, err, "  -A                       Opt in to call-argument values (may contain sensitive data)\n", stderr);
    p101_fputs(env, err, "  -R                       Opt in to call-result values (may contain sensitive data)\n", stderr);
    p101_fputs(env, err, "  -o <capture-dir>         Directory to create for captured evidence\n", stderr);
    p101_fputs(env, err, "                           (default: p101-observe-<pid>)\n", stderr);
    p101_fputs(env, err, "\nUse `p101 run` to capture and analyze in one command.\n", stderr);
#else
    (void)exit_code;
    (void)message;
    (void)program_name;
#endif
}
