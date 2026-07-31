#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <inttypes.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c/p101_time.h>
#include <p101_posix/p101_time.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_stat.h>
#include <p101_posix/sys/p101_utsname.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <time.h>

void p101_observe_make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths)
{
    struct timespec now;
    intmax_t        seconds;
    long            pid;

    P101_TRACE_SCOPE(env);
    pid = p101_getpid(env);
    p101_clock_gettime(env, err, CLOCK_REALTIME, &now);
    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- clock failure is an OS-level defensive path
    {
        goto done;    // GCOVR_EXCL_LINE
    }
    seconds = now.tv_sec;
    p101_snprintf(env, err, paths->run_id, sizeof(paths->run_id), "p101-%ld-%jd-%09ld", pid, seconds, now.tv_nsec);

    if(args->report_dir == NULL)
    {
        p101_snprintf(env, err, paths->dir, sizeof(paths->dir), "%s-%ld", DEFAULT_REPORT_PREFIX, pid);
    }
    else
    {
        p101_strncpy(env, paths->dir, args->report_dir, sizeof(paths->dir) - 1U);
        paths->dir[sizeof(paths->dir) - 1U] = '\0';
    }

    p101_observe_join_path(env, err, paths->command, paths->dir, "command.txt");
    p101_observe_join_path(env, err, paths->stdout_text, paths->dir, "stdout.txt");
    p101_observe_join_path(env, err, paths->stderr_text, paths->dir, "stderr.txt");
    p101_observe_join_path(env, err, paths->resource_log, paths->dir, "resources.log");
    p101_observe_join_path(env, err, paths->call_log, paths->dir, "calls.log");
    p101_observe_join_path(env, err, paths->resource_report, paths->dir, "resource-report.txt");
    p101_observe_join_path(env, err, paths->resource_json, paths->dir, "resource-report.json");
    p101_observe_join_path(env, err, paths->resource_stderr, paths->dir, "resource-tools.stderr.txt");
    p101_observe_join_path(env, err, paths->concurrency_report, paths->dir, "concurrency-report.txt");
    p101_observe_join_path(env, err, paths->concurrency_json, paths->dir, "concurrency-report.json");
    p101_observe_join_path(env, err, paths->concurrency_stderr, paths->dir, "concurrency-tools.stderr.txt");
    p101_observe_join_path(env, err, paths->trace_tree, paths->dir, "trace-tree.txt");
    p101_observe_join_path(env, err, paths->trace_summary, paths->dir, "trace-summary.txt");
    p101_observe_join_path(env, err, paths->trace_stderr, paths->dir, "trace-tools.stderr.txt");
    p101_observe_join_path(env, err, paths->correlated_report, paths->dir, "correlated-report.txt");
    p101_observe_join_path(env, err, paths->correlated_json, paths->dir, "correlated-report.json");
    p101_observe_join_path(env, err, paths->correlated_mermaid, paths->dir, "resource-lifetimes.md");
    p101_observe_join_path(env, err, paths->run_model, paths->dir, "run-model.json");
    p101_observe_join_path(env, err, paths->report_driver_output, paths->dir, "report-driver.stdout.txt");
    p101_observe_join_path(env, err, paths->report_stderr, paths->dir, "report-tools.stderr.txt");
    p101_observe_join_path(env, err, paths->summary, paths->dir, "summary.txt");
    p101_observe_join_path(env, err, paths->manifest, paths->dir, "manifest.txt");
    p101_observe_join_path(env, err, paths->receipt, paths->dir, "receipt.txt");

done:
    return;
}

void p101_observe_join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE_SCOPE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written < 0 || written >= PATH_LEN)    // GCOVR_EXCL_BR_LINE -- snprintf negative result is wrapper-level failure
    {
        P101_ERROR_RAISE_USER(err, "A report path is too long.", ERR_USAGE);
    }
}

void p101_observe_create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE_SCOPE(env);
    p101_mkdir(env, err, path, REPORT_DIR_MODE);
}

void p101_observe_create_empty_file(const struct p101_env *env, struct p101_error *err, const char *path)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_observe_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
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

void p101_observe_write_manifest_file(const struct p101_env *env, struct p101_error *err, const char *path, const struct arguments *args, const struct report_paths *paths)
{
    FILE          *stream;
    struct utsname host;
    time_t         generated_at;
    intmax_t       generated_at_value;
    bool           have_time;
    int            have_host;
    const char    *argument_logging;
    const char    *result_logging;

    P101_TRACE_SCOPE(env);
    stream           = p101_fopen(env, err, path, "w");
    argument_logging = "redacted";
    result_logging   = "redacted";
    if(args->log_arguments)
    {
        argument_logging = "enabled";
    }
    if(args->log_results)
    {
        result_logging = "enabled";
    }

    if(stream == NULL)
    {
        goto done;
    }

    generated_at       = p101_time(env, err, NULL);
    have_time          = p101_error_has_no_error(err);
    generated_at_value = generated_at;

    // GCOVR_EXCL_START -- OS time failure fallback
    if(!have_time)
    {
        p101_error_reset(err);
    }
    // GCOVR_EXCL_STOP

    have_host = p101_uname(env, err, &host);

    // GCOVR_EXCL_START -- OS uname failure fallback
    if(have_host != 0)
    {
        p101_error_reset(err);
    }
    // GCOVR_EXCL_STOP

    p101_fputs(env, err, "p101-observe manifest\n", stream);
    p101_fputs(env, err, "schema=p101-observe-manifest-v2\n", stream);
    p101_fprintf(env, err, stream, "run_id=%s\n", paths->run_id);
    p101_fputs(env, err, "event_schema=" P101_TOOL_EVENT_SCHEMA_NAME "\n", stream);
    p101_fprintf(env, err, stream, "event_log_version=%d\n", P101_TOOL_EVENT_LOG_VERSION);
    p101_fputs(env, err, "event_timestamp_fields=sequence,monotonic_ns,wall_unix_ns\n", stream);
    if(have_time)    // GCOVR_EXCL_BR_LINE -- false only after the excluded OS fallback
    {
        p101_fprintf(env, err, stream, "generated_at_unix=%jd\n", generated_at_value);
    }
    if(have_host == 0)    // GCOVR_EXCL_BR_LINE -- false only after the excluded OS fallback
    {
        p101_fprintf(env, err, stream, "host_sysname=%s\n", host.sysname);
        p101_fprintf(env, err, stream, "host_release=%s\n", host.release);
        p101_fprintf(env, err, stream, "host_machine=%s\n", host.machine);
    }
    p101_fprintf(env, err, stream, "report_dir=%s\n", paths->dir);
    p101_fprintf(env, err, stream, "resource_tracker=%s\n", args->resource_tracker);
    p101_fprintf(env, err, stream, "p101_sync_check=%s\n", args->p101_sync_check);
    p101_fprintf(env, err, stream, "p101_trace=%s\n", args->p101_trace);
    p101_fprintf(env, err, stream, "p101_report=%s\n", args->p101_report);
    p101_fprintf(env, err, stream, "call_arguments=%s\n", argument_logging);
    p101_fprintf(env, err, stream, "call_results=%s\n", result_logging);
    p101_fprintf(env, err, stream, "resource_log=%s\n", paths->resource_log);
    p101_fprintf(env, err, stream, "call_log=%s\n", paths->call_log);
    p101_fputs(env, err, "command=", stream);

    for(size_t i = 0; args->command_argv[i] != NULL; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0U) ? "" : " ", args->command_argv[i]);
    }

    p101_fputc(env, err, '\n', stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
