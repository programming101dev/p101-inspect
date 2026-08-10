#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <errno.h>
#include <inttypes.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c/p101_time.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>
#include <p101_host/p101_stdlib.h>
#include <p101_host/p101_unistd.h>
#include <p101_host/sys/p101_utsname.h>
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
#include <p101_time/p101_time.h>
#include <p101_tool_event/event.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <time.h>

static void create_directory_component(const struct p101_env *env, struct p101_error *err, const char *path)
{
    struct p101_error *optional_error;
    int                result;
    int                actual_error;

    optional_error = P101_ERROR_OPTIONAL;
    result         = p101_mkdir(env, optional_error, path, REPORT_DIR_MODE);
    actual_error   = errno;

    if(result == -1 && actual_error != EEXIST)
    {
        P101_ERROR_RAISE_ERRNO(err, actual_error);
    }
}

void p101_observe_make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths)
{
    bool            p101_call_result_1;
    struct timespec now;
    intmax_t        seconds;
    long            pid;

    P101_TRACE_SCOPE(env);
    pid = p101_getpid(env);
    p101_clock_gettime(env, err, CLOCK_REALTIME, &now);
    // GCOVR_EXCL_BR_START: clock failure is an OS-level defensive path.
    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        goto done;    // GCOVR_EXCL_LINE
    }
    // GCOVR_EXCL_BR_STOP
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
    p101_observe_join_path(env, err, paths->summary, paths->dir, "summary.txt");
    p101_observe_join_path(env, err, paths->manifest, paths->dir, "manifest.txt");
    p101_observe_join_path(env, err, paths->receipt, paths->dir, "receipt.txt");
    p101_observe_join_path(env, err, paths->tool_receipt, paths->dir, "tool-receipt.json");

done:
    return;
}

void p101_observe_join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE_SCOPE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    // GCOVR_EXCL_START: a negative result is a wrapper-level failure;
    // truncation remains a directly tested admitted input.
    if(written < 0)
    {
        P101_ERROR_RAISE_USER(err, "Could not format a report path.", ERR_USAGE);
    }
    // GCOVR_EXCL_STOP
    else if(written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "A report path is too long.", ERR_USAGE);
    }
}

void p101_observe_create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    char   path_copy[PATH_LEN];
    size_t length;
    bool   has_error;

    P101_TRACE_SCOPE(env);
    length = p101_strlen(env, path);
    if(length >= sizeof(path_copy))
    {
        P101_ERROR_RAISE_USER(err, "The report directory path is too long.", ERR_USAGE);
        goto done;
    }

    p101_memcpy(env, path_copy, path, length + 1U);
    for(size_t index = 0U; index <= length; index++)
    {
        char separator;

        if(path_copy[index] != '/' && path_copy[index] != '\0')
        {
            continue;
        }
        if(index == 0U || path_copy[index - 1U] == '/')
        {
            continue;
        }

        separator        = path_copy[index];
        path_copy[index] = '\0';
        create_directory_component(env, err, path_copy);
        path_copy[index] = separator;

        has_error = p101_error_has_error(err);
        if(has_error)
        {
            goto done;
        }
    }

done:
    return;
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

    p101_fputs(env, err, "inspect-capture manifest\n", stream);
    p101_fputs(env, err, "schema=inspect-capture-manifest-v2\n", stream);
    p101_fprintf(env, err, stream, "run_id=%s\n", paths->run_id);
    p101_fputs(env, err, "event_schema=" P101_TOOL_EVENT_SCHEMA_NAME "\n", stream);
    p101_fprintf(env, err, stream, "event_log_version=%d\n", P101_TOOL_EVENT_LOG_VERSION);
    p101_fputs(env, err, "event_timestamp_fields=sequence,monotonic_ns,wall_unix_ns\n", stream);
    // GCOVR_EXCL_BR_START: false only after the excluded OS fallback.
    if(have_time)
    {
        p101_fprintf(env, err, stream, "generated_at_unix=%jd\n", generated_at_value);
    }
    // GCOVR_EXCL_BR_STOP
    // GCOVR_EXCL_BR_START: false only after the excluded OS fallback.
    if(have_host == 0)
    {
        p101_fprintf(env, err, stream, "host_sysname=%s\n", host.sysname);
        p101_fprintf(env, err, stream, "host_release=%s\n", host.release);
        p101_fprintf(env, err, stream, "host_machine=%s\n", host.machine);
    }
    // GCOVR_EXCL_BR_STOP
    p101_fprintf(env, err, stream, "report_dir=%s\n", paths->dir);
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
