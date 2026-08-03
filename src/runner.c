#include "runner.h"
#include "child_execution.h"
#include "paths.h"
#include "report.h"
#include "status.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdlib.h>

static int finish_capture(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result)
{
    p101_observe_write_summary_file(env, err, args, paths, result);
    p101_observe_write_receipt_file(env, err, paths, result);
    if(p101_error_has_error(err))
    {
        return EXIT_TROUBLE;
    }
    p101_printf(env, err, "p101-observe: captured run in %s\n", paths->dir);
    if(p101_observe_status_is_success(result->command_status))
    {
        return EXIT_SUCCESS;
    }
    return EXIT_FINDINGS;
}

#ifdef P101_OBSERVE_TESTING
int p101_observe_test_finish_capture_only(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result)
{
    return finish_capture(env, err, args, paths, result);
}
#endif

int p101_observe_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_paths   paths;
    struct observe_result result;
    int                   ret_val;

    P101_TRACE_SCOPE(env);
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
    result.command_status = p101_observe_run_observed_command(env, err, args, &paths);
    if(p101_error_has_error(err))
    {
        goto done;
    }
    ret_val = finish_capture(env, err, args, &paths, &result);

done:
    return ret_val;
}
