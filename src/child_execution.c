#include "child_execution.h"
#include "constants.h"
#include <p101_c/p101_stdlib.h>
#include <p101_process/p101_sched.h>
#include <p101_process/p101_setjmp.h>
#include <p101_process/p101_signal.h>
#include <p101_process/p101_spawn.h>
#include <p101_process/p101_stdio.h>
#include <p101_process/p101_stdlib.h>
#include <p101_process/p101_unistd.h>
#include <p101_process/sys/p101_resource.h>
#include <p101_process/sys/p101_times.h>
#include <p101_process/sys/p101_wait.h>
#include <p101_util/tool_run.h>

struct observed_child_context
{
    const struct arguments    *args;
    const struct report_paths *paths;
};

static void setup_observed_child(const struct p101_env *env, struct p101_error *err, void *context);
static void setup_helper_child(const struct p101_env *env, struct p101_error *err, void *context);
static void set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths);
static void set_fault_environment_from_parent_request(const struct p101_env *env, struct p101_error *err);
static void setenv_if_present(const struct p101_env *env, struct p101_error *err, const char *source_name, const char *target_name);
static void clear_observe_environment(const struct p101_env *env, struct p101_error *err);
static void clear_helper_environment(const struct p101_env *env, struct p101_error *err);

#ifdef P101_OBSERVE_TESTING
void p101_observe_test_redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path)
{
    p101_tool_run_redirect(env, err, stdout_path, stderr_path, REPORT_FILE_MODE);
}

void p101_observe_test_set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths)
{
    set_observed_environment(env, err, args, paths);
}

void p101_observe_test_set_fault_environment(const struct p101_env *env, struct p101_error *err)
{
    set_fault_environment_from_parent_request(env, err);
}

void p101_observe_test_setenv_if_present(const struct p101_env *env, struct p101_error *err, const char *source_name, const char *target_name)
{
    setenv_if_present(env, err, source_name, target_name);
}

void p101_observe_test_clear_observe_environment(const struct p101_env *env, struct p101_error *err)
{
    clear_observe_environment(env, err);
}

void p101_observe_test_clear_helper_environment(const struct p101_env *env, struct p101_error *err)
{
    clear_helper_environment(env, err);
}

void p101_observe_test_setup_observed_child(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths)
{
    struct observed_child_context context;

    context.args  = args;
    context.paths = paths;
    setup_observed_child(env, err, &context);
}

void p101_observe_test_setup_helper_child(const struct p101_env *env, struct p101_error *err)
{
    setup_helper_child(env, err, NULL);
}
#endif

int p101_observe_run_observed_command(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths)
{
    struct observed_child_context context;
    struct p101_tool_run_options  options;

    P101_TRACE_SCOPE(env);
    context.args                = args;
    context.paths               = paths;
    options.stdout_path         = paths->stdout_text;
    options.stderr_path         = paths->stderr_text;
    options.diagnostic_name     = "p101-observe";
    options.output_mode         = REPORT_FILE_MODE;
    options.child_setup         = setup_observed_child;
    options.child_setup_context = &context;
    return p101_tool_run_capture(env, err, args->command_argv, &options);
}

int p101_observe_run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path)
{
    struct p101_tool_run_options options;

    P101_TRACE_SCOPE(env);
    options.stdout_path         = stdout_path;
    options.stderr_path         = stderr_path;
    options.diagnostic_name     = "p101-observe";
    options.output_mode         = REPORT_FILE_MODE;
    options.child_setup         = setup_helper_child;
    options.child_setup_context = NULL;
    return p101_tool_run_capture(env, err, tool_argv, &options);
}

static void setup_observed_child(const struct p101_env *env, struct p101_error *err, void *context)
{
    const struct observed_child_context *child;

    child = (const struct observed_child_context *)context;
    clear_observe_environment(env, err);
    set_observed_environment(env, err, child->args, child->paths);
}

static void setup_helper_child(const struct p101_env *env, struct p101_error *err, void *context)
{
    (void)context;
    clear_helper_environment(env, err);
}

static void set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths)
{
    P101_TRACE_SCOPE(env);
    p101_setenv(env, err, P101_ENV_EVENT_RUN_ID_ENV, paths->run_id, 1);
    p101_setenv(env, err, RESOURCE_LOG_ENV, paths->resource_log, 1);
    p101_setenv(env, err, CALL_LOG_ENV, paths->call_log, 1);
    if(args->log_arguments)
    {
        p101_setenv(env, err, CALL_LOG_ARGS_ENV, "1", 1);
    }
    if(args->log_results)
    {
        p101_setenv(env, err, CALL_LOG_RESULT_ENV, "1", 1);
    }
    set_fault_environment_from_parent_request(env, err);
}

static void set_fault_environment_from_parent_request(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
    value = p101_getenv(env, err, source_name);

    if(value != NULL && value[0] != '\0')
    {
        p101_setenv(env, err, target_name, value, 1);
    }
}

static void clear_observe_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE_SCOPE(env);
    p101_unsetenv(env, err, P101_ENV_EVENT_RUN_ID_ENV);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ARGS_ENV);
    p101_unsetenv(env, err, CALL_LOG_RESULT_ENV);
}

static void clear_helper_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE_SCOPE(env);
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
