#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <inttypes.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c/p101_time.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_stat.h>
#include <p101_posix/sys/p101_utsname.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <time.h>

void p101_observe_make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths)
{
    P101_TRACE(env);

    if(args->report_dir == NULL)
    {
        p101_snprintf(env, err, paths->dir, sizeof(paths->dir), "%s-%ld", DEFAULT_REPORT_PREFIX, (long)p101_getpid(env));
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
    p101_observe_join_path(env, err, paths->trace_tree, paths->dir, "trace-tree.txt");
    p101_observe_join_path(env, err, paths->trace_summary, paths->dir, "trace-summary.txt");
    p101_observe_join_path(env, err, paths->trace_stderr, paths->dir, "trace-tools.stderr.txt");
    p101_observe_join_path(env, err, paths->correlated_report, paths->dir, "correlated-report.txt");
    p101_observe_join_path(env, err, paths->correlated_json, paths->dir, "correlated-report.json");
    p101_observe_join_path(env, err, paths->correlated_mermaid, paths->dir, "resource-lifetimes.md");
    p101_observe_join_path(env, err, paths->report_stderr, paths->dir, "report-tools.stderr.txt");
    p101_observe_join_path(env, err, paths->summary, paths->dir, "summary.txt");
    p101_observe_join_path(env, err, paths->manifest, paths->dir, "manifest.txt");
}

void p101_observe_join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written < 0 || written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "A report path is too long.", ERR_USAGE);
    }
}

void p101_observe_create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE(env);
    p101_mkdir(env, err, path, REPORT_DIR_MODE);
}

void p101_observe_create_empty_file(const struct p101_env *env, struct p101_error *err, const char *path)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_observe_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
{
    FILE *stream;

    P101_TRACE(env);
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
    bool           have_time;
    int            have_host;

    P101_TRACE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream == NULL)
    {
        goto done;
    }

    generated_at = p101_time(env, err, NULL);
    have_time    = p101_error_has_no_error(err);

    if(!have_time)
    {
        p101_error_reset(err);
        generated_at = (time_t)0;
    }

    have_host = p101_uname(env, err, &host);

    if(have_host != 0)
    {
        p101_error_reset(err);
    }

    p101_fputs(env, err, "p101-observe manifest\n", stream);
    p101_fputs(env, err, "event_schema=p101-event-format-v2\n", stream);
    p101_fputs(env, err, "event_log_version=2\n", stream);
    p101_fputs(env, err, "event_timestamp_fields=sequence,monotonic_ns,wall_unix_ns\n", stream);
    if(have_time)
    {
        p101_fprintf(env, err, stream, "generated_at_unix=%jd\n", (intmax_t)generated_at);
    }
    if(have_host == 0)
    {
        p101_fprintf(env, err, stream, "host_sysname=%s\n", host.sysname);
        p101_fprintf(env, err, stream, "host_release=%s\n", host.release);
        p101_fprintf(env, err, stream, "host_machine=%s\n", host.machine);
    }
    p101_fprintf(env, err, stream, "report_dir=%s\n", paths->dir);
    p101_fprintf(env, err, stream, "resource_tracker=%s\n", args->resource_tracker);
    p101_fprintf(env, err, stream, "p101_trace=%s\n", args->p101_trace);
    p101_fprintf(env, err, stream, "p101_report=%s\n", args->p101_report);
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
