#include "runner.h"
#include "constants.h"
#include "paths.h"
#include "report.h"
#include "resource.h"
#include "status.h"
#include <fcntl.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_fcntl.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_wait.h>
#include <stdio.h>

static int  run_observed_command(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths);
static int  run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path);
static void redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path);
static void set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct report_paths *paths);
static void set_fault_environment_from_parent_request(const struct p101_env *env, struct p101_error *err);
static void setenv_if_present(const struct p101_env *env, struct p101_error *err, const char *source_name, const char *target_name);
static void clear_observe_environment(const struct p101_env *env, struct p101_error *err);
static void clear_helper_environment(const struct p101_env *env, struct p101_error *err);

int p101_observe_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_paths   paths;
    struct observe_result result;
    char                 *resource_argv[3];
    char                 *resource_json_argv[4];
    char                 *trace_tree_argv[3];
    char                 *trace_summary_argv[4];
    char                 *report_argv[3];
    char                 *report_json_argv[4];
    char                 *report_mermaid_argv[4];
    char                  json_option[]    = "-j";
    char                  mermaid_option[] = "-m";
    char                  summary_option[] = "-s";
    char                  resource_tracker_path[PATH_LEN];
    char                  trace_path[PATH_LEN];
    char                  report_path[PATH_LEN];
    int                   ret_val;

    P101_TRACE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    p101_observe_make_report_paths(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_observe_create_report_dir(env, err, paths.dir);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_observe_write_command_file(env, err, paths.command, args->command_argv);
    p101_observe_write_manifest_file(env, err, paths.manifest, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_observe_create_empty_file(env, err, paths.resource_log);
    p101_observe_create_empty_file(env, err, paths.call_log);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_strncpy(env, resource_tracker_path, args->resource_tracker, sizeof(resource_tracker_path) - 1U);
    resource_tracker_path[sizeof(resource_tracker_path) - 1U] = '\0';
    p101_strncpy(env, trace_path, args->p101_trace, sizeof(trace_path) - 1U);
    trace_path[sizeof(trace_path) - 1U] = '\0';
    p101_strncpy(env, report_path, args->p101_report, sizeof(report_path) - 1U);
    report_path[sizeof(report_path) - 1U] = '\0';

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

    p101_observe_read_resource_json(env, err, paths.resource_json, &result.resources);

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

    report_argv[0]       = report_path;
    report_argv[1]       = paths.dir;
    report_argv[2]       = NULL;
    result.report_status = run_tool_capture(env, err, report_argv, paths.correlated_report, paths.report_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    report_json_argv[0]       = report_path;
    report_json_argv[1]       = json_option;
    report_json_argv[2]       = paths.dir;
    report_json_argv[3]       = NULL;
    result.report_json_status = run_tool_capture(env, err, report_json_argv, paths.correlated_json, paths.report_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    report_mermaid_argv[0]       = report_path;
    report_mermaid_argv[1]       = mermaid_option;
    report_mermaid_argv[2]       = paths.dir;
    report_mermaid_argv[3]       = NULL;
    result.report_mermaid_status = run_tool_capture(env, err, report_mermaid_argv, paths.correlated_mermaid, paths.report_stderr);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_observe_write_summary_file(env, err, args, &paths, &result);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_observe_write_receipt_file(env, err, &paths, &result);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_printf(env, err, "p101-observe: wrote report to %s\n", paths.dir);

    if(!p101_observe_tool_status_is_acceptable(result.resource_status) || !p101_observe_tool_status_is_acceptable(result.resource_json_status) || !p101_observe_tool_status_is_acceptable(result.trace_tree_status) ||
       !p101_observe_tool_status_is_acceptable(result.trace_summary_status) || !p101_observe_tool_status_is_acceptable(result.report_status) || !p101_observe_tool_status_is_acceptable(result.report_json_status) ||
       !p101_observe_tool_status_is_acceptable(result.report_mermaid_status))
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if(!result.resources.parsed)
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if(!p101_observe_status_is_success(result.command_status) || p101_observe_resource_finding_count(&result.resources) > 0U)
    {
        ret_val = EXIT_FINDINGS;
        goto done;
    }

    ret_val = EXIT_SUCCESS;

done:
    return ret_val;
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
            p101_posix_exit_immediately(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, args->command_argv[0], args->command_argv);
        p101_fprintf(env, err, stderr, "p101-observe: exec failed for %s: %s\n", args->command_argv[0], p101_error_get_message(err));
        p101_posix_exit_immediately(env, EXEC_FAILURE);
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
        clear_helper_environment(env, err);
        redirect_child_output(env, err, stdout_path, stderr_path);

        if(p101_error_has_error(err))
        {
            p101_fprintf(env, err, stderr, "p101-observe: tool setup failed: %s\n", p101_error_get_message(err));
            p101_posix_exit_immediately(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, tool_argv[0], tool_argv);
        p101_fprintf(env, err, stderr, "p101-observe: exec failed for %s: %s\n", tool_argv[0], p101_error_get_message(err));
        p101_posix_exit_immediately(env, EXEC_FAILURE);
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
    p101_setenv(env, err, EVENT_LOG_VERSION_ENV, EVENT_LOG_VERSION_VALUE, 1);
    p101_setenv(env, err, CALL_LOG_ARGS_ENV, "1", 1);
    p101_setenv(env, err, CALL_LOG_RESULT_ENV, "1", 1);
    set_fault_environment_from_parent_request(env, err);
}

static void set_fault_environment_from_parent_request(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    setenv_if_present(env, err, CHILD_FAULT_CALL_ENV, FAULT_CALL_ENV);
    setenv_if_present(env, err, CHILD_FAULT_ERRNO_ENV, FAULT_ERRNO_ENV);
    setenv_if_present(env, err, CHILD_FAULT_LOG_ENV, FAULT_LOG_ENV);
    setenv_if_present(env, err, CHILD_FAULT_NAME_ENV, FAULT_NAME_ENV);
    setenv_if_present(env, err, CHILD_FAULT_MODE_ENV, FAULT_MODE_ENV);
    setenv_if_present(env, err, CHILD_FAULT_AMOUNT_ENV, FAULT_AMOUNT_ENV);
    setenv_if_present(env, err, CHILD_FAULT_REPEAT_ENV, FAULT_REPEAT_ENV);
}

static void setenv_if_present(const struct p101_env *env, struct p101_error *err, const char *source_name, const char *target_name)
{
    const char *value;

    P101_TRACE(env);
    value = p101_getenv(env, source_name);

    if(value != NULL && value[0] != '\0')
    {
        p101_setenv(env, err, target_name, value, 1);
    }
}

static void clear_observe_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ENV);
    p101_unsetenv(env, err, EVENT_LOG_VERSION_ENV);
    p101_unsetenv(env, err, CALL_LOG_ARGS_ENV);
    p101_unsetenv(env, err, CALL_LOG_RESULT_ENV);
}

static void clear_helper_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    clear_observe_environment(env, err);
    p101_unsetenv(env, err, FAULT_CALL_ENV);
    p101_unsetenv(env, err, FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, FAULT_LOG_ENV);
    p101_unsetenv(env, err, FAULT_NAME_ENV);
    p101_unsetenv(env, err, FAULT_MODE_ENV);
    p101_unsetenv(env, err, FAULT_AMOUNT_ENV);
    p101_unsetenv(env, err, FAULT_REPEAT_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_CALL_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_LOG_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_NAME_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_MODE_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_AMOUNT_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_REPEAT_ENV);
}
