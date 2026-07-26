#include "arguments.h"
#include "errors.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_fcntl.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_stat.h>
#include <p101_posix/sys/p101_wait.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

struct resource_summary
{
    size_t records;
    size_t fd_leaks;
    size_t allocation_leaks;
    size_t bad_releases;
    bool   parsed;
};

struct report_paths
{
    char dir[PATH_LEN];
    char command[PATH_LEN];
    char stdout_text[PATH_LEN];
    char stderr_text[PATH_LEN];
    char resource_log[PATH_LEN];
    char call_log[PATH_LEN];
    char resource_report[PATH_LEN];
    char resource_json[PATH_LEN];
    char resource_stderr[PATH_LEN];
    char trace_tree[PATH_LEN];
    char trace_summary[PATH_LEN];
    char trace_stderr[PATH_LEN];
    char summary[PATH_LEN];
};

struct observe_result
{
    int                     command_status;
    int                     resource_status;
    int                     resource_json_status;
    int                     trace_tree_status;
    int                     trace_summary_status;
    struct resource_summary resources;
};

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static int  run_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args);

static void make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths);
static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
static void create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path);
static void create_empty_file(const struct p101_env *env, struct p101_error *err, const char *path);
static void write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *command_argv);
static void write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result);

static int  run_observed_command(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths);
static int  run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path);
static void redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path);
static void set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct report_paths *paths);
static void clear_observe_environment(const struct p101_env *env, struct p101_error *err);

static void           read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary);
static bool           parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary);
static bool           parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value);
static bool           status_is_success(int status);
static bool           tool_status_is_acceptable(int status);
static void           print_status(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
static size_t         resource_finding_count(const struct resource_summary *summary);
_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

static const char DEFAULT_REPORT_PREFIX[] = "p101-observe";
static const char DEFAULT_TRACKER_PATH[]  = "resource-tracker";
static const char DEFAULT_TRACE_PATH[]    = "p101-trace";
static const char RESOURCE_LOG_ENV[]      = "P101_RESOURCE_LOG";
static const char CALL_LOG_ENV[]          = "P101_CALL_LOG";
static const char CALL_LOG_ARGS_ENV[]     = "P101_CALL_LOG_ARGS";
static const char CALL_LOG_RESULT_ENV[]   = "P101_CALL_LOG_RESULT";
static const char JSON_RECORDS[]          = "\"records\"";
static const char JSON_FD_LEAKS[]         = "\"fd_leaks\"";
static const char JSON_ALLOCATION_LEAKS[] = "\"allocation_leaks\"";
static const char JSON_BAD_RELEASES[]     = "\"bad_releases\"";

enum
{
    MSG_LEN          = 256,
    READ_BUF_LEN     = 4096,
    JSON_BUF_LEN     = 65536,
    JSON_NUMBER_BASE = 10,
    EXEC_FAILURE     = 127,
    REPORT_DIR_MODE  = 0755,
    REPORT_FILE_MODE = 0644,
    EXIT_FINDINGS    = 1,
    EXIT_TROUBLE     = 2
};

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    int                ret_val;

    ret_val = EXIT_TROUBLE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    p101_memset(env, &args, 0, sizeof(args));
    args.resource_tracker = DEFAULT_TRACKER_PATH;
    args.p101_trace       = DEFAULT_TRACE_PATH;

    parse_arguments(env, err, argc, argv, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args.verbose)
    {
        p101_env_set_tracer(env, p101_env_default_tracer);
    }

    check_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = run_observe(env, err, &args);

done:
    if(p101_error_has_error(err))
    {
        if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
        {
            const char *msg;

            msg = p101_error_get_message(err);
            usage(env, err, argv[0], EXIT_TROUBLE, msg);
        }

        p101_fprintf(env, err, stderr, "%s\n", p101_error_get_message(err));
        ret_val = EXIT_TROUBLE;
    }

    p101_env_destroy(env);
    p101_error_destroy(err);

    return ret_val;
}

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvo:r:t:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
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
            case 't':
            {
                args->p101_trace = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
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
            default:
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c' returned by getopt.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_no_error(err))
    {
        args->command_argv = &argv[optind];
    }
}

static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE(env);

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

    if(args->resource_tracker == NULL || args->resource_tracker[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The resource-tracker path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_trace == NULL || args->p101_trace[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-trace path must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

static int run_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_paths   paths;
    struct observe_result result;
    char                 *resource_argv[3];
    char                 *resource_json_argv[4];
    char                 *trace_tree_argv[3];
    char                 *trace_summary_argv[4];
    char                  json_option[]    = "-j";
    char                  summary_option[] = "-s";
    char                  resource_tracker_path[PATH_LEN];
    char                  trace_path[PATH_LEN];
    int                   ret_val;

    P101_TRACE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    make_report_paths(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    create_report_dir(env, err, paths.dir);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    write_command_file(env, err, paths.command, args->command_argv);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    create_empty_file(env, err, paths.resource_log);
    create_empty_file(env, err, paths.call_log);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_strncpy(env, resource_tracker_path, args->resource_tracker, sizeof(resource_tracker_path) - 1U);
    resource_tracker_path[sizeof(resource_tracker_path) - 1U] = '\0';
    p101_strncpy(env, trace_path, args->p101_trace, sizeof(trace_path) - 1U);
    trace_path[sizeof(trace_path) - 1U] = '\0';

    result.command_status = run_observed_command(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    resource_argv[0]       = resource_tracker_path;
    resource_argv[1]       = paths.resource_log;
    resource_argv[2]       = NULL;
    result.resource_status = run_tool_capture(env, err, resource_argv, paths.resource_report, paths.resource_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    resource_json_argv[0]       = resource_tracker_path;
    resource_json_argv[1]       = json_option;
    resource_json_argv[2]       = paths.resource_log;
    resource_json_argv[3]       = NULL;
    result.resource_json_status = run_tool_capture(env, err, resource_json_argv, paths.resource_json, paths.resource_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    read_resource_json(env, err, paths.resource_json, &result.resources);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    trace_tree_argv[0]       = trace_path;
    trace_tree_argv[1]       = paths.call_log;
    trace_tree_argv[2]       = NULL;
    result.trace_tree_status = run_tool_capture(env, err, trace_tree_argv, paths.trace_tree, paths.trace_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    trace_summary_argv[0]       = trace_path;
    trace_summary_argv[1]       = summary_option;
    trace_summary_argv[2]       = paths.call_log;
    trace_summary_argv[3]       = NULL;
    result.trace_summary_status = run_tool_capture(env, err, trace_summary_argv, paths.trace_summary, paths.trace_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    write_summary_file(env, err, args, &paths, &result);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_printf(env, err, "p101-observe: wrote report to %s\n", paths.dir);

    if(!tool_status_is_acceptable(result.resource_status) || !tool_status_is_acceptable(result.resource_json_status) || !tool_status_is_acceptable(result.trace_tree_status) || !tool_status_is_acceptable(result.trace_summary_status))
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if(!status_is_success(result.command_status) || resource_finding_count(&result.resources) > 0U)
    {
        ret_val = EXIT_FINDINGS;
        goto done;
    }

    ret_val = EXIT_SUCCESS;

done:
    return ret_val;
}

static void make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths)
{
    P101_TRACE(env);

    if(args->report_dir == NULL)
    {
        p101_snprintf(env, err, paths->dir, sizeof(paths->dir), "%s-%ld", DEFAULT_REPORT_PREFIX, (long)p101_getpid(env));
    }
    else
    {
        p101_strncpy(env, paths->dir, args->report_dir, sizeof(paths->dir) - 1U);
        paths->dir[sizeof(paths->dir) - 1U] = '\0';
    }

    join_path(env, err, paths->command, paths->dir, "command.txt");
    join_path(env, err, paths->stdout_text, paths->dir, "stdout.txt");
    join_path(env, err, paths->stderr_text, paths->dir, "stderr.txt");
    join_path(env, err, paths->resource_log, paths->dir, "resources.log");
    join_path(env, err, paths->call_log, paths->dir, "calls.log");
    join_path(env, err, paths->resource_report, paths->dir, "resource-report.txt");
    join_path(env, err, paths->resource_json, paths->dir, "resource-report.json");
    join_path(env, err, paths->resource_stderr, paths->dir, "resource-tools.stderr.txt");
    join_path(env, err, paths->trace_tree, paths->dir, "trace-tree.txt");
    join_path(env, err, paths->trace_summary, paths->dir, "trace-summary.txt");
    join_path(env, err, paths->trace_stderr, paths->dir, "trace-tools.stderr.txt");
    join_path(env, err, paths->summary, paths->dir, "summary.txt");
}

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written < 0 || written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "A report path is too long.", ERR_USAGE);
    }
}

static void create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE(env);
    p101_mkdir(env, err, path, REPORT_DIR_MODE);
}

static void create_empty_file(const struct p101_env *env, struct p101_error *err, const char *path)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream == NULL)
    {
        goto done;
    }

    for(size_t i = 0; command_argv[i] != NULL; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0U) ? "" : " ", command_argv[i]);
    }

    p101_fputc(env, err, '\n', stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, paths->summary, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "p101-observe summary\n", stream);
    p101_fputs(env, err, "====================\n\n", stream);
    p101_fprintf(env, err, stream, "command: %s\n", args->command_argv[0]);
    p101_fprintf(env, err, stream, "report_dir: %s\n\n", paths->dir);
    print_status(env, err, stream, "command", result->command_status);
    print_status(env, err, stream, "resource-tracker", result->resource_status);
    print_status(env, err, stream, "resource-tracker-json", result->resource_json_status);
    print_status(env, err, stream, "p101-trace-tree", result->trace_tree_status);
    print_status(env, err, stream, "p101-trace-summary", result->trace_summary_status);

    if(result->resources.parsed)
    {
        p101_fprintf(env, err, stream, "\nresources: records=%zu fd_leaks=%zu allocation_leaks=%zu bad_releases=%zu\n", result->resources.records, result->resources.fd_leaks, result->resources.allocation_leaks, result->resources.bad_releases);
    }
    else
    {
        p101_fputs(env, err, "\nresources: unavailable\n", stream);
    }

    p101_fputs(env, err, "\nfiles:\n", stream);
    p101_fprintf(env, err, stream, "  stdout: %s\n", paths->stdout_text);
    p101_fprintf(env, err, stream, "  stderr: %s\n", paths->stderr_text);
    p101_fprintf(env, err, stream, "  resources: %s\n", paths->resource_log);
    p101_fprintf(env, err, stream, "  calls: %s\n", paths->call_log);
    p101_fprintf(env, err, stream, "  resource_report: %s\n", paths->resource_report);
    p101_fprintf(env, err, stream, "  resource_json: %s\n", paths->resource_json);
    p101_fprintf(env, err, stream, "  trace_tree: %s\n", paths->trace_tree);
    p101_fprintf(env, err, stream, "  trace_summary: %s\n", paths->trace_summary);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static int run_observed_command(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths)
{
    int   status;
    pid_t pid;

    P101_TRACE(env);
    status = 0;
    pid    = p101_fork(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(pid == 0)
    {
        clear_observe_environment(env, err);
        redirect_child_output(env, err, paths->stdout_text, paths->stderr_text);
        set_observed_environment(env, err, paths);

        if(p101_error_has_error(err))
        {
            p101_fprintf(env, err, stderr, "p101-observe: child setup failed: %s\n", p101_error_get_message(err));
            p101__exit(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, args->command_argv[0], args->command_argv);
        p101_fprintf(env, err, stderr, "p101-observe: exec failed for %s: %s\n", args->command_argv[0], p101_error_get_message(err));
        p101__exit(env, EXEC_FAILURE);
    }

    p101_waitpid(env, err, pid, &status, 0);

done:
    return status;
}

static int run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path)
{
    int   status;
    pid_t pid;

    P101_TRACE(env);
    status = 0;
    pid    = p101_fork(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(pid == 0)
    {
        clear_observe_environment(env, err);
        redirect_child_output(env, err, stdout_path, stderr_path);

        if(p101_error_has_error(err))
        {
            p101_fprintf(env, err, stderr, "p101-observe: tool setup failed: %s\n", p101_error_get_message(err));
            p101__exit(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, tool_argv[0], tool_argv);
        p101_fprintf(env, err, stderr, "p101-observe: exec failed for %s: %s\n", tool_argv[0], p101_error_get_message(err));
        p101__exit(env, EXEC_FAILURE);
    }

    p101_waitpid(env, err, pid, &status, 0);

done:
    return status;
}

static void redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path)
{
    int stdout_fd;
    int stderr_fd;

    P101_TRACE(env);
    stdout_fd = p101_open(env, err, stdout_path, O_WRONLY | O_CREAT | O_TRUNC, REPORT_FILE_MODE);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    stderr_fd = p101_open(env, err, stderr_path, O_WRONLY | O_CREAT | O_TRUNC, REPORT_FILE_MODE);

    if(p101_error_has_error(err))
    {
        p101_close(env, err, stdout_fd);
        goto done;
    }

    p101_dup2(env, err, stdout_fd, STDOUT_FILENO);
    p101_dup2(env, err, stderr_fd, STDERR_FILENO);
    p101_close(env, err, stdout_fd);
    p101_close(env, err, stderr_fd);

done:
    return;
}

static void set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct report_paths *paths)
{
    P101_TRACE(env);
    p101_setenv(env, err, RESOURCE_LOG_ENV, paths->resource_log, 1);
    p101_setenv(env, err, CALL_LOG_ENV, paths->call_log, 1);
    p101_setenv(env, err, CALL_LOG_ARGS_ENV, "1", 1);
    p101_setenv(env, err, CALL_LOG_RESULT_ENV, "1", 1);
}

static void clear_observe_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ARGS_ENV);
    p101_unsetenv(env, err, CALL_LOG_RESULT_ENV);
}

static void read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary)
{
    FILE  *stream;
    char   buffer[JSON_BUF_LEN];
    size_t used;

    P101_TRACE(env);
    p101_memset(env, summary, 0, sizeof(*summary));
    stream = p101_fopen(env, err, path, "r");
    used   = 0;

    if(stream == NULL)
    {
        goto done;
    }

    while(p101_error_has_no_error(err) && used < sizeof(buffer) - 1U)
    {
        const char *line;

        line = p101_fgets(env, err, buffer + used, (int)(sizeof(buffer) - used), stream);

        if(line == NULL)
        {
            break;
        }

        used = p101_strlen(env, buffer);
    }

    buffer[used] = '\0';
    (void)parse_resource_summary(env, buffer, summary);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static bool parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary)
{
    bool parsed;
    bool records_parsed;
    bool fd_leaks_parsed;
    bool allocation_leaks_parsed;
    bool bad_releases_parsed;

    records_parsed          = parse_json_size(env, text, JSON_RECORDS, &summary->records);
    fd_leaks_parsed         = parse_json_size(env, text, JSON_FD_LEAKS, &summary->fd_leaks);
    allocation_leaks_parsed = parse_json_size(env, text, JSON_ALLOCATION_LEAKS, &summary->allocation_leaks);
    bad_releases_parsed     = parse_json_size(env, text, JSON_BAD_RELEASES, &summary->bad_releases);
    parsed                  = (records_parsed && fd_leaks_parsed && allocation_leaks_parsed && bad_releases_parsed) != 0;
    summary->parsed         = parsed;

    return parsed;
}

static bool parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value)
{
    const char   *cursor;
    char         *end;
    unsigned long parsed;
    bool          ok;

    ok     = false;
    cursor = p101_strstr(env, text, key);

    if(cursor == NULL)
    {
        goto done;
    }

    cursor = p101_strchr(env, cursor, ':');

    if(cursor == NULL)
    {
        goto done;
    }

    cursor++;
    parsed = p101_strtoul(env, NULL, cursor, &end, JSON_NUMBER_BASE);

    if(cursor == end)
    {
        goto done;
    }

    *value = (size_t)parsed;
    ok     = true;

done:
    return ok;
}

static bool status_is_success(int status)
{
    bool success;

    success = false;

    if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
    {
        success = true;
    }

    return success;
}

static bool tool_status_is_acceptable(int status)
{
    bool acceptable;

    acceptable = false;

    if(WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS || WEXITSTATUS(status) == EXIT_FINDINGS))
    {
        acceptable = true;
    }

    return acceptable;
}

static void print_status(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        p101_fprintf(env, err, stream, "%s: exit=%d\n", label, WEXITSTATUS(status));
    }
    else if(WIFSIGNALED(status))
    {
        p101_fprintf(env, err, stream, "%s: signal=%d\n", label, WTERMSIG(status));
    }
    else
    {
        p101_fprintf(env, err, stream, "%s: status=%d\n", label, status);
    }
}

static size_t resource_finding_count(const struct resource_summary *summary)
{
    size_t count;

    count = 0;

    if(summary->parsed)
    {
        count = summary->fd_leaks + summary->allocation_leaks + summary->bad_releases;
    }

    return count;
}

_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE(env);

#ifndef P101_SUPPRESS_USAGE_TEXT
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-o <report-dir>] [-r <resource-tracker>] [-t <p101-trace>] -- <command> [args...]\n", program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                       Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                       Enable verbose p101 tracing in p101-observe\n", stderr);
    p101_fputs(env, err, "  -o <report-dir>          Directory to create for the report\n", stderr);
    p101_fputs(env, err, "                           (default: p101-observe-<pid>)\n", stderr);
    p101_fputs(env, err, "  -r <resource-tracker>    resource-tracker executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -t <p101-trace>          p101-trace executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "\nThe child should use p101_env_create() from an updated lib_env build.\n", stderr);
#else
    (void)message;
    (void)program_name;
#endif

    p101_exit(env, exit_code);
}
