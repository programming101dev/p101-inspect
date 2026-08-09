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
    bool p101_call_result_1;
    int  status;

    p101_observe_write_summary_file(env, err, args, paths, result);
    p101_observe_write_receipt_file(env, err, paths, result);
    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        status = EXIT_TROUBLE;
    }
    else
    {
        bool p101_call_result_2;

        p101_printf(env, err, "inspect-capture: captured run in %s\n", paths->dir);
        p101_call_result_2 = p101_observe_status_is_success(result->command_status);
        if(p101_call_result_2)
        {
            status = EXIT_SUCCESS;
        }
        else
        {
            status = EXIT_FINDINGS;
        }
    }

    return status;
}

#ifdef P101_OBSERVE_TESTING
int p101_observe_test_finish_capture_only(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result)
{
    return finish_capture(env, err, args, paths, result);
}
#endif

int p101_observe_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    bool                  p101_call_result_3;
    bool                  p101_call_result_4;
    bool                  p101_call_result_5;
    bool                  p101_call_result_6;
    bool                  p101_call_result_7;
    struct report_paths   paths;
    struct observe_result result;
    int                   ret_val;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    p101_observe_make_report_paths(env, err, args, &paths);
    p101_call_result_3 = p101_error_has_error(err);
    if(p101_call_result_3)
    {
        goto done;
    }
    p101_observe_create_report_dir(env, err, paths.dir);
    p101_call_result_4 = p101_error_has_error(err);
    if(p101_call_result_4)
    {
        goto done;
    }
    p101_observe_write_command_file(env, err, paths.command, args->command_argv);
    p101_observe_write_manifest_file(env, err, paths.manifest, args, &paths);
    p101_call_result_5 = p101_error_has_error(err);
    if(p101_call_result_5)
    {
        goto done;
    }
    p101_observe_create_empty_file(env, err, paths.resource_log);
    p101_observe_create_empty_file(env, err, paths.call_log);
    p101_call_result_6 = p101_error_has_error(err);
    if(p101_call_result_6)
    {
        goto done;
    }
    result.command_status = p101_observe_run_observed_command(env, err, args, &paths);
    p101_call_result_7    = p101_error_has_error(err);
    if(p101_call_result_7)
    {
        goto done;
    }
    ret_val = finish_capture(env, err, args, &paths, &result);

done:
    return ret_val;
}
