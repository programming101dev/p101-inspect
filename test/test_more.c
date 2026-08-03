#include "arguments.h"
#include "cli.h"
#include "constants.h"
#include "paths.h"
#include "report.h"
#include "status.h"
#include "unity.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_filesystem/filesystem.h>
#include <p101_io/io.h>
#include <p101_process/process.h>
#include <sys/wait.h>

static struct p101_error *more_error;
static struct p101_env   *more_env;

void p101_observe_test_redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path);
void p101_observe_test_set_observed_environment(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths);
void p101_observe_test_set_fault_environment(const struct p101_env *env, struct p101_error *err);
void p101_observe_test_setenv_if_present(const struct p101_env *env, struct p101_error *err, const char *source_name, const char *target_name);
void p101_observe_test_clear_observe_environment(const struct p101_env *env, struct p101_error *err);
void p101_observe_test_clear_helper_environment(const struct p101_env *env, struct p101_error *err);
void p101_observe_test_setup_observed_child(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths);
void p101_observe_test_setup_helper_child(const struct p101_env *env, struct p101_error *err);
int p101_observe_test_finish_capture_only(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result);

static void reset_error(void)
{
    if(p101_error_has_error(more_error))
    {
        p101_error_reset(more_error);
    }
}

static void test_status_variants(void)
{
    FILE *stream;

    TEST_ASSERT_TRUE(p101_observe_status_is_success(0));
    TEST_ASSERT_FALSE(p101_observe_status_is_success(1 << 8));
    TEST_ASSERT_FALSE(p101_observe_status_is_success(SIGTERM));
    TEST_ASSERT_TRUE(p101_observe_tool_status_is_acceptable(0));
    TEST_ASSERT_TRUE(p101_observe_tool_status_is_acceptable(1 << 8));
    TEST_ASSERT_FALSE(p101_observe_tool_status_is_acceptable(2 << 8));
    TEST_ASSERT_FALSE(p101_observe_tool_status_is_acceptable(SIGTERM));

    stream = p101_tmpfile(more_env, more_error);
    p101_observe_print_status(more_env, more_error, stream, "exit", 2 << 8);
    p101_observe_print_status(more_env, more_error, stream, "signal", SIGTERM);
    p101_observe_print_status(more_env, more_error, stream, "raw", 0x7f);
    p101_observe_print_status(more_env, more_error, stream, "continued", 4991);
    p101_fclose(more_env, more_error, stream);
}

static void test_argument_validation_variants(void)
{
    struct arguments args;
    char            *command[] = {"command", NULL};

    p101_memset(more_env, &args, 0, sizeof(args));
    p101_observe_check_arguments(more_env, more_error, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    args.command_argv = command;
    p101_observe_check_arguments(more_env, more_error, &args);
    TEST_ASSERT_TRUE(p101_error_has_no_error(more_error));

#define REJECT(field, value)                                                                                                                                                                                                                                       \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        const char *saved = args.field;                                                                                                                                                                                                                            \
        args.field        = value;                                                                                                                                                                                                                                 \
        p101_observe_check_arguments(more_env, more_error, &args);                                                                                                                                                                                                 \
        TEST_ASSERT_TRUE(p101_error_has_error(more_error));                                                                                                                                                                                                        \
        reset_error();                                                                                                                                                                                                                                             \
        args.field = saved;                                                                                                                                                                                                                                        \
    } while(0)
    REJECT(report_dir, "");
#undef REJECT
}

static void test_path_and_report_writers(void)
{
    struct arguments      args;
    struct report_paths   paths;
    struct observe_result result;
    char                  dir[]     = "/tmp/p101-observe-writers-XXXXXX";
    char                 *command[] = {"demo", "argument", NULL};

    TEST_ASSERT_NOT_NULL(p101_mkdtemp(more_env, more_error, dir));
    p101_memset(more_env, &args, 0, sizeof(args));
    p101_memset(more_env, &paths, 0, sizeof(paths));
    p101_memset(more_env, &result, 0, sizeof(result));
    args.command_argv     = command;
    args.log_arguments    = true;
    args.log_results      = true;
    args.report_dir       = dir;
    p101_observe_make_report_paths(more_env, more_error, &args, &paths);

    p101_observe_create_empty_file(more_env, more_error, paths.resource_log);
    p101_observe_create_empty_file(more_env, more_error, paths.call_log);
    p101_observe_write_command_file(more_env, more_error, paths.command, command);
    p101_observe_write_manifest_file(more_env, more_error, paths.manifest, &args, &paths);
    p101_observe_write_summary_file(more_env, more_error, &args, &paths, &result);
    p101_observe_write_receipt_file(more_env, more_error, &paths, &result);
    reset_error();
    result.command_status = SIGTERM;
    p101_observe_write_receipt_file(more_env, more_error, &paths, &result);
    reset_error();
    result.command_status = 4991;
    p101_observe_write_receipt_file(more_env, more_error, &paths, &result);
    reset_error();

    args.report_dir = NULL;
    p101_observe_make_report_paths(more_env, more_error, &args, &paths);
    TEST_ASSERT_EQUAL_STRING_LEN(DEFAULT_REPORT_PREFIX, paths.dir, sizeof(DEFAULT_REPORT_PREFIX) - 1U);

    p101_observe_create_empty_file(more_env, more_error, "/definitely/missing/file");
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    p101_observe_write_command_file(more_env, more_error, "/definitely/missing/file", command);
    reset_error();
    p101_observe_write_manifest_file(more_env, more_error, "/definitely/missing/file", &args, &paths);
    reset_error();
    p101_strncpy(more_env, paths.summary, "/definitely/missing/summary", sizeof(paths.summary) - 1U);
    p101_observe_write_summary_file(more_env, more_error, &args, &paths, &result);
    reset_error();
    p101_strncpy(more_env, paths.receipt, "/definitely/missing/receipt", sizeof(paths.receipt) - 1U);
    p101_observe_write_receipt_file(more_env, more_error, &paths, &result);
    reset_error();
}

static void test_runner_environment_helpers(void)
{
    struct arguments    args;
    struct report_paths paths;
    char                out_path[] = "/tmp/p101-observe-stdout-XXXXXX";
    char                err_path[] = "/tmp/p101-observe-stderr-XXXXXX";
    int                 out_fd;
    int                 err_fd;
    int                 saved_stdout;
    int                 saved_stderr;

    p101_memset(more_env, &args, 0, sizeof(args));
    p101_memset(more_env, &paths, 0, sizeof(paths));
    p101_strncpy(more_env, paths.run_id, "observe-test-run", sizeof(paths.run_id) - 1U);
    p101_strncpy(more_env, paths.resource_log, "resources", sizeof(paths.resource_log) - 1U);
    p101_strncpy(more_env, paths.call_log, "calls", sizeof(paths.call_log) - 1U);

    p101_observe_test_setenv_if_present(more_env, more_error, "P101_TEST_MISSING", "P101_TEST_TARGET");
    p101_setenv(more_env, more_error, "P101_TEST_SOURCE", "", 1);
    p101_observe_test_setenv_if_present(more_env, more_error, "P101_TEST_SOURCE", "P101_TEST_TARGET");
    p101_setenv(more_env, more_error, "P101_TEST_SOURCE", "value", 1);
    p101_observe_test_setenv_if_present(more_env, more_error, "P101_TEST_SOURCE", "P101_TEST_TARGET");
    TEST_ASSERT_EQUAL_STRING("value", p101_getenv(more_env, more_error, "P101_TEST_TARGET"));
    p101_unsetenv(more_env, more_error, "P101_TEST_SOURCE");
    p101_unsetenv(more_env, more_error, "P101_TEST_TARGET");

    p101_observe_test_set_observed_environment(more_env, more_error, &args, &paths);
    TEST_ASSERT_EQUAL_STRING("observe-test-run", p101_getenv(more_env, more_error, P101_ENV_EVENT_RUN_ID_ENV));
    args.log_arguments = true;
    args.log_results   = true;
    p101_observe_test_set_observed_environment(more_env, more_error, &args, &paths);
    p101_observe_test_setup_observed_child(more_env, more_error, &args, &paths);
    p101_observe_test_setup_helper_child(more_env, more_error);

    p101_setenv(more_env, more_error, CHILD_FAULT_CALL_ENV, "3", 1);
    p101_setenv(more_env, more_error, CHILD_FAULT_ERRNO_ENV, "5", 1);
    p101_setenv(more_env, more_error, CHILD_FAULT_LOG_ENV, "fault.log", 1);
    p101_setenv(more_env, more_error, CHILD_FAULT_NAME_ENV, "read", 1);
    p101_setenv(more_env, more_error, CHILD_FAULT_MODE_ENV, "short", 1);
    p101_setenv(more_env, more_error, CHILD_FAULT_AMOUNT_ENV, "2", 1);
    p101_setenv(more_env, more_error, CHILD_FAULT_REPEAT_ENV, "2", 1);
    p101_observe_test_set_fault_environment(more_env, more_error);
    p101_observe_test_clear_observe_environment(more_env, more_error);
    p101_observe_test_clear_helper_environment(more_env, more_error);

    out_fd = p101_mkstemp(more_env, more_error, out_path);
    err_fd = p101_mkstemp(more_env, more_error, err_path);
    p101_close(more_env, more_error, out_fd);
    p101_close(more_env, more_error, err_fd);
    saved_stdout = p101_dup(more_env, more_error, STDOUT_FILENO);
    saved_stderr = p101_dup(more_env, more_error, STDERR_FILENO);
    p101_observe_test_redirect_child_output(more_env, more_error, out_path, err_path);
    p101_dup2(more_env, more_error, saved_stdout, STDOUT_FILENO);
    p101_dup2(more_env, more_error, saved_stderr, STDERR_FILENO);
    p101_close(more_env, more_error, saved_stdout);
    p101_close(more_env, more_error, saved_stderr);
    p101_unlink(more_env, more_error, out_path);
    p101_unlink(more_env, more_error, err_path);

    p101_observe_test_redirect_child_output(more_env, more_error, "/missing/out", "/missing/err");
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    out_fd = p101_mkstemp(more_env, more_error, out_path);
    p101_close(more_env, more_error, out_fd);
    p101_observe_test_redirect_child_output(more_env, more_error, out_path, "/missing/err");
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    p101_unlink(more_env, more_error, out_path);
}

static void test_runner_result_predicates(void)
{
    struct arguments      args;
    struct report_paths   paths;
    struct observe_result result;

    p101_memset(more_env, &result, 0, sizeof(result));
    p101_memset(more_env, &args, 0, sizeof(args));
    p101_memset(more_env, &paths, 0, sizeof(paths));

    p101_strncpy(more_env, paths.summary, "/missing/p101-observe-summary", sizeof(paths.summary) - 1U);
    p101_strncpy(more_env, paths.receipt, "/missing/p101-observe-receipt", sizeof(paths.receipt) - 1U);
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_observe_test_finish_capture_only(more_env, more_error, &args, &paths, &result));
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
}

void p101_observe_run_more_tests(void)
{
    more_error = p101_error_create(false);
    more_env   = p101_env_create(more_error, NULL);
    RUN_TEST(test_status_variants);
    RUN_TEST(test_argument_validation_variants);
    RUN_TEST(test_path_and_report_writers);
    RUN_TEST(test_runner_environment_helpers);
    RUN_TEST(test_runner_result_predicates);
    p101_env_destroy(more_env);
    p101_error_destroy(more_error);
}
