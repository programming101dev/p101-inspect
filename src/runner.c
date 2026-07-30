#include "runner.h"
#include "child_execution.h"
#include "constants.h"
#include "paths.h"
#include "report.h"
#include "resource.h"
#include "status.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>
#include <stdlib.h>
static bool tool_statuses_are_acceptable(const struct observe_result *result);
static bool result_has_findings(const struct observe_result *result);

#ifdef P101_OBSERVE_TESTING
bool p101_observe_test_tool_statuses_are_acceptable(const struct observe_result *result)
{
    return tool_statuses_are_acceptable(result);
}

bool p101_observe_test_result_has_findings(const struct observe_result *result)
{
    return result_has_findings(result);
}
#endif

static bool tool_statuses_are_acceptable(const struct observe_result *result)
{
    const int statuses[] = {result->resource_status,
                            result->resource_json_status,
                            result->concurrency_status,
                            result->concurrency_json_status,
                            result->trace_tree_status,
                            result->trace_summary_status,
                            result->report_status,
                            result->report_json_status,
                            result->report_mermaid_status};

    for(size_t index = 0U; index < sizeof(statuses) / sizeof(statuses[0]); index++)
    {
        if(!p101_observe_tool_status_is_acceptable(statuses[index]))
        {
            return false;
        }
    }
    return true;
}

static bool result_has_findings(const struct observe_result *result)
{
    const int statuses[] = {result->command_status,
                            result->resource_status,
                            result->resource_json_status,
                            result->concurrency_status,
                            result->concurrency_json_status,
                            result->trace_tree_status,
                            result->trace_summary_status,
                            result->report_status,
                            result->report_json_status,
                            result->report_mermaid_status};

    if(p101_observe_resource_finding_count(&result->resources) > 0U)
    {
        return true;
    }
    for(size_t index = 0U; index < sizeof(statuses) / sizeof(statuses[0]); index++)
    {
        if(!p101_observe_status_is_success(statuses[index]))
        {
            return true;
        }
    }
    return false;
}

int p101_observe_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_paths   paths;
    struct observe_result result;
    char                 *resource_argv[3];
    char                 *resource_json_argv[4];
    char                 *concurrency_argv[3];
    char                 *concurrency_json_argv[4];
    char                 *trace_tree_argv[3];
    char                 *trace_summary_argv[4];
    char                 *report_argv[REPORT_TOOL_ARGV_LEN];
    char                  json_option[]    = "-j";
    char                  bundle_option[]  = "-b";
    char                  summary_option[] = "-s";
    char                  resource_tracker_path[PATH_LEN];
    char                  concurrency_path[PATH_LEN];
    char                  trace_path[PATH_LEN];
    char                  report_path[PATH_LEN];
    int                   ret_val;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    p101_observe_make_report_paths(env, err, args, &paths);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_observe_create_report_dir(env, err, paths.dir);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_observe_write_command_file(env, err, paths.command, args->command_argv);
    p101_observe_write_manifest_file(env, err, paths.manifest, args, &paths);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_observe_create_empty_file(env, err, paths.resource_log);
    p101_observe_create_empty_file(env, err, paths.call_log);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_strncpy(env, resource_tracker_path, args->resource_tracker, sizeof(resource_tracker_path) - 1U);
    resource_tracker_path[sizeof(resource_tracker_path) - 1U] = '\0';
    p101_strncpy(env, concurrency_path, args->p101_sync_check, sizeof(concurrency_path) - 1U);
    concurrency_path[sizeof(concurrency_path) - 1U] = '\0';
    p101_strncpy(env, trace_path, args->p101_trace, sizeof(trace_path) - 1U);
    trace_path[sizeof(trace_path) - 1U] = '\0';
    p101_strncpy(env, report_path, args->p101_report, sizeof(report_path) - 1U);
    report_path[sizeof(report_path) - 1U] = '\0';

    result.command_status = p101_observe_run_observed_command(env, err, args, &paths);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    if(args->capture_only)
    {
        p101_observe_write_summary_file(env, err, args, &paths, &result);
        p101_observe_write_receipt_file(env, err, &paths, &result);
        if(p101_error_has_error(err))
        {
            goto done;
        }
        p101_printf(env, err, "p101-observe: captured run in %s\n", paths.dir);
        if(p101_observe_status_is_success(result.command_status))
        {
            ret_val = EXIT_SUCCESS;
        }
        else
        {
            ret_val = EXIT_FINDINGS;
        }
        goto done;
    }

    result.analysis_ran       = true;
    concurrency_argv[0]       = concurrency_path;
    concurrency_argv[1]       = paths.resource_log;
    concurrency_argv[2]       = NULL;
    result.concurrency_status = p101_observe_run_tool_capture(env, err, concurrency_argv, paths.concurrency_report, paths.concurrency_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    concurrency_json_argv[0]       = concurrency_path;
    concurrency_json_argv[1]       = json_option;
    concurrency_json_argv[2]       = paths.resource_log;
    concurrency_json_argv[3]       = NULL;
    result.concurrency_json_status = p101_observe_run_tool_capture(env, err, concurrency_json_argv, paths.concurrency_json, paths.concurrency_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    resource_argv[0]       = resource_tracker_path;
    resource_argv[1]       = paths.resource_log;
    resource_argv[2]       = NULL;
    result.resource_status = p101_observe_run_tool_capture(env, err, resource_argv, paths.resource_report, paths.resource_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    resource_json_argv[0]       = resource_tracker_path;
    resource_json_argv[1]       = json_option;
    resource_json_argv[2]       = paths.resource_log;
    resource_json_argv[3]       = NULL;
    result.resource_json_status = p101_observe_run_tool_capture(env, err, resource_json_argv, paths.resource_json, paths.resource_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_observe_read_resource_json(env, err, paths.resource_json, &result.resources);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    trace_tree_argv[0]       = trace_path;
    trace_tree_argv[1]       = paths.call_log;
    trace_tree_argv[2]       = NULL;
    result.trace_tree_status = p101_observe_run_tool_capture(env, err, trace_tree_argv, paths.trace_tree, paths.trace_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    trace_summary_argv[0]       = trace_path;
    trace_summary_argv[1]       = summary_option;
    trace_summary_argv[2]       = paths.call_log;
    trace_summary_argv[3]       = NULL;
    result.trace_summary_status = p101_observe_run_tool_capture(env, err, trace_summary_argv, paths.trace_summary, paths.trace_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    report_argv[0]       = report_path;
    report_argv[1]       = bundle_option;
    report_argv[2]       = paths.dir;
    report_argv[3]       = paths.dir;
    report_argv[4]       = NULL;
    result.report_status = p101_observe_run_tool_capture(env, err, report_argv, paths.report_driver_output, paths.report_stderr);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    result.report_json_status    = result.report_status;
    result.report_mermaid_status = result.report_status;

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_observe_write_summary_file(env, err, args, &paths, &result);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_observe_write_receipt_file(env, err, &paths, &result);

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- defensive propagation after wrapper failure
    {
        goto done;    // GCOVR_EXCL_LINE
    }

    p101_printf(env, err, "p101-observe: wrote report to %s\n", paths.dir);

    if(!tool_statuses_are_acceptable(&result))
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if(!result.resources.parsed || !result.resources.log_complete)
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if(result_has_findings(&result))
    {
        ret_val = EXIT_FINDINGS;
        goto done;
    }

    ret_val = EXIT_SUCCESS;

done:
    return ret_val;
}
