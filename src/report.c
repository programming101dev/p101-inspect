#include "report.h"
#include "resource.h"
#include "status.h"
#include <p101_c/p101_stdio.h>
#include <stdio.h>

void p101_observe_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result)
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
    p101_observe_print_status(env, err, stream, "command", result->command_status);
    p101_observe_print_status(env, err, stream, "p101-resource-tracker", result->resource_status);
    p101_observe_print_status(env, err, stream, "p101-resource-tracker-json", result->resource_json_status);
    p101_observe_print_status(env, err, stream, "p101-trace-tree", result->trace_tree_status);
    p101_observe_print_status(env, err, stream, "p101-trace-summary", result->trace_summary_status);
    p101_observe_print_status(env, err, stream, "p101-report", result->report_status);
    p101_observe_print_status(env, err, stream, "p101-report-json", result->report_json_status);

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
    p101_fprintf(env, err, stream, "  resource_tools_stderr: %s\n", paths->resource_stderr);
    p101_fprintf(env, err, stream, "  trace_tree: %s\n", paths->trace_tree);
    p101_fprintf(env, err, stream, "  trace_summary: %s\n", paths->trace_summary);
    p101_fprintf(env, err, stream, "  trace_tools_stderr: %s\n", paths->trace_stderr);
    p101_fprintf(env, err, stream, "  correlated_report: %s\n", paths->correlated_report);
    p101_fprintf(env, err, stream, "  correlated_json: %s\n", paths->correlated_json);
    p101_fprintf(env, err, stream, "  report_tools_stderr: %s\n", paths->report_stderr);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
