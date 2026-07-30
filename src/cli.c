#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <stdlib.h>

void p101_observe_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->resource_tracker = DEFAULT_TRACKER_PATH;
    args->p101_sync_check  = DEFAULT_CONCURRENCY_PATH;
    args->p101_trace       = DEFAULT_TRACE_PATH;
    args->p101_report      = DEFAULT_REPORT_PATH;
}

void p101_observe_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE_SCOPE(env);
    opterr = 0;

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        p101_observe_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
    }

    while((opt = p101_getopt(env, argc, argv, ":hvCARo:r:d:t:p:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)    // GCOVR_EXCL_BR_LINE -- default is a defensive p101_getopt contract check
        {
            case 'h':
            {
                p101_observe_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'C':
            {
                args->capture_only = true;
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
            case 'r':
            {
                args->resource_tracker = optarg;
                break;
            }
            case 'd':
            {
                args->p101_sync_check = optarg;
                break;
            }
            case 't':
            {
                args->p101_trace = optarg;
                break;
            }
            case 'p':
            {
                args->p101_report = optarg;
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

    if(!args->capture_only && (args->resource_tracker == NULL || args->resource_tracker[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-resource-tracker path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!args->capture_only && (args->p101_trace == NULL || args->p101_trace[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-trace path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!args->capture_only && (args->p101_sync_check == NULL || args->p101_sync_check[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-sync-check path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!args->capture_only && (args->p101_report == NULL || args->p101_report[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-report path must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

_Noreturn void p101_observe_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE_SCOPE(env);

#ifndef P101_SUPPRESS_USAGE_TEXT
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-C] [-A] [-R] [-o <report-dir>] [-r <p101-resource-tracker>] [-d <p101-sync-check>] [-t <p101-trace>] [-p <p101-report>] -- <command> [args...]\n", program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                       Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                       Enable verbose p101 tracing in p101-observe\n", stderr);
    p101_fputs(env, err, "  -C                       Capture only; defer all offline analysis\n", stderr);
    p101_fputs(env, err, "  -A                       Opt in to call-argument values (may contain sensitive data)\n", stderr);
    p101_fputs(env, err, "  -R                       Opt in to call-result values (may contain sensitive data)\n", stderr);
    p101_fputs(env, err, "  -o <report-dir>          Directory to create for the report\n", stderr);
    p101_fputs(env, err, "                           (default: p101-observe-<pid>)\n", stderr);
    p101_fputs(env, err, "  -r <p101-resource-tracker>    p101-resource-tracker executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -d <p101-sync-check>    p101-sync-check executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -t <p101-trace>          p101-trace executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -p <p101-report>         p101-report executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "\nThe child should use p101_env_create() from an updated lib_env build.\n", stderr);
#else
    (void)message;
    (void)program_name;
#endif

    p101_exit(env, exit_code);
}
