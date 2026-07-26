#define main p101_test_unused_main
#include "../src/main.c"
#undef main

#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>

static struct p101_error *error;
static struct p101_env   *env;

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

static void test_parse_accepts_report_tools_and_command(void)
{
    char            *argv[] = {"p101-observe", "-o", "report", "-r", "rt", "-t", "trace", "--", "prog", "arg", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    args.resource_tracker = DEFAULT_TRACKER_PATH;
    args.p101_trace       = DEFAULT_TRACE_PATH;

    parse_arguments(env, error, 10, argv, &args);
    check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("report", args.report_dir);
    TEST_ASSERT_EQUAL_STRING("rt", args.resource_tracker);
    TEST_ASSERT_EQUAL_STRING("trace", args.p101_trace);
    TEST_ASSERT_EQUAL_STRING("prog", args.command_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg", args.command_argv[1]);
}

static void test_parse_rejects_missing_command(void)
{
    char            *argv[] = {"p101-observe", "-o", "report", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    args.resource_tracker = DEFAULT_TRACKER_PATH;
    args.p101_trace       = DEFAULT_TRACE_PATH;

    parse_arguments(env, error, 3, argv, &args);
    check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_parse_resource_summary_accepts_tracker_json(void)
{
    const char              json[] = "{\"summary\":{\"records\":7,\"fd_leaks\":1,\"allocation_leaks\":2,\"bad_releases\":3}}";
    struct resource_summary summary;

    p101_memset(env, &summary, 0, sizeof(summary));

    TEST_ASSERT_TRUE(parse_resource_summary(env, json, &summary));
    TEST_ASSERT_TRUE(summary.parsed);
    TEST_ASSERT_EQUAL_UINT(7U, summary.records);
    TEST_ASSERT_EQUAL_UINT(1U, summary.fd_leaks);
    TEST_ASSERT_EQUAL_UINT(2U, summary.allocation_leaks);
    TEST_ASSERT_EQUAL_UINT(3U, summary.bad_releases);
}

static void test_join_path_rejects_long_paths(void)
{
    char destination[PATH_LEN];
    char long_dir[PATH_LEN];

    reset_getopt();
    p101_memset(env, long_dir, 'a', sizeof(long_dir));
    long_dir[sizeof(long_dir) - 1U] = '\0';

    join_path(env, error, destination, long_dir, "file.txt");

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_report_tools_and_command);
    RUN_TEST(test_parse_rejects_missing_command);
    RUN_TEST(test_parse_resource_summary_accepts_tracker_json);
    RUN_TEST(test_join_path_rejects_long_paths);
    return UNITY_END();
}
