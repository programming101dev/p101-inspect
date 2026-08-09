#include "cli.h"
#include "constants.h"
#include "errors.h"
#include "paths.h"
#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <unistd.h>

static struct p101_error *error;
static struct p101_env   *env;

void p101_observe_run_more_tests(void);

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void reset_getopt(void)
{
#ifdef __GLIBC__
    optind = 0;
#else
    extern int optreset;
    optreset = 1;
    optind   = 1;
#endif
}

static void test_parse_accepts_capture_directory_and_command(void)
{
    char            *argv[] = {"inspect-capture", "-o", "report", "--", "prog", "arg", NULL};
    struct arguments args;

    reset_getopt();
    p101_observe_arguments_init(env, &args);

    p101_observe_parse_arguments(env, error, 6, argv, &args);
    p101_observe_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("report", args.report_dir);
    TEST_ASSERT_EQUAL_STRING("prog", args.command_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg", args.command_argv[1]);
}

static void test_call_values_are_opt_in(void)
{
    char            *argv[] = {"inspect-capture", "-A", "-R", "--", "prog", NULL};
    struct arguments args;

    reset_getopt();
    p101_observe_arguments_init(env, &args);
    TEST_ASSERT_FALSE(args.log_arguments);
    TEST_ASSERT_FALSE(args.log_results);

    p101_observe_parse_arguments(env, error, 5, argv, &args);
    p101_observe_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(args.log_arguments);
    TEST_ASSERT_TRUE(args.log_results);
}

static void test_parse_rejects_missing_command(void)
{
    char            *argv[] = {"inspect-capture", "-o", "report", NULL};
    struct arguments args;

    reset_getopt();
    p101_observe_arguments_init(env, &args);

    p101_observe_parse_arguments(env, error, 3, argv, &args);
    p101_observe_check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_parse_rejects_retired_analyzer_option(void)
{
    char            *argv[] = {"inspect-capture", "-p", "retired", "--", "prog", NULL};
    struct arguments args;

    reset_getopt();
    p101_observe_arguments_init(env, &args);

    p101_observe_parse_arguments(env, error, 5, argv, &args);
    p101_observe_check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_join_path_rejects_long_paths(void)
{
    char destination[PATH_LEN];
    char long_dir[PATH_LEN];

    reset_getopt();
    p101_memset(env, long_dir, 'a', sizeof(long_dir));
    long_dir[sizeof(long_dir) - 1U] = '\0';

    p101_observe_join_path(env, error, destination, long_dir, "file.txt");

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_make_report_paths_includes_manifest_and_graph(void)
{
    struct arguments    args;
    struct report_paths paths;

    p101_memset(env, &args, 0, sizeof(args));
    p101_memset(env, &paths, 0, sizeof(paths));
    args.report_dir = "/tmp/inspect-capture-test";

    p101_observe_make_report_paths(env, error, &args, &paths);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING_LEN("p101-", paths.run_id, 5U);
    TEST_ASSERT_EQUAL_STRING("/tmp/inspect-capture-test/manifest.txt", paths.manifest);
    TEST_ASSERT_EQUAL_STRING("/tmp/inspect-capture-test/receipt.txt", paths.receipt);
    TEST_ASSERT_EQUAL_STRING("/tmp/inspect-capture-test/tool-receipt.json", paths.tool_receipt);
    TEST_ASSERT_EQUAL_STRING("/tmp/inspect-capture-test/resources.log", paths.resource_log);
    TEST_ASSERT_EQUAL_STRING("/tmp/inspect-capture-test/calls.log", paths.call_log);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_capture_directory_and_command);
    RUN_TEST(test_call_values_are_opt_in);
    RUN_TEST(test_parse_rejects_missing_command);
    RUN_TEST(test_parse_rejects_retired_analyzer_option);
    RUN_TEST(test_join_path_rejects_long_paths);
    RUN_TEST(test_make_report_paths_includes_manifest_and_graph);
    p101_observe_run_more_tests();
    return UNITY_END();
}
