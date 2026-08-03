#include "report.h"
#include "status.h"
#include <inttypes.h>
#include <p101_c/p101_stdio.h>
#include <p101_tool_event/event.h>
#include <p101_tool_event/receipt.h>
#include <stdio.h>
#include <sys/wait.h>

static void write_artifact_fingerprint(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *role, const char *path);
static void write_status(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *role, int status);

void p101_observe_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->summary, "w");

    if(stream == NULL)
    {
        goto done;    // GCOVR_EXCL_LINE -- fopen failure propagation
    }

    p101_fputs(env, err, "p101-observe summary\n", stream);
    p101_fputs(env, err, "====================\n\n", stream);
    p101_fprintf(env, err, stream, "command: %s\n", args->command_argv[0]);
    p101_fprintf(env, err, stream, "report_dir: %s\n\n", paths->dir);
    p101_fputs(env, err, "analysis: deferred to p101 analyze\n", stream);
    p101_observe_print_status(env, err, stream, "command", result->command_status);

    p101_fputs(env, err, "\nfiles:\n", stream);
    p101_fprintf(env, err, stream, "  stdout: %s\n", paths->stdout_text);
    p101_fprintf(env, err, stream, "  stderr: %s\n", paths->stderr_text);
    p101_fprintf(env, err, stream, "  resources: %s\n", paths->resource_log);
    p101_fprintf(env, err, stream, "  calls: %s\n", paths->call_log);
    p101_fprintf(env, err, stream, "  manifest: %s\n", paths->manifest);
    p101_fprintf(env, err, stream, "  receipt: %s\n", paths->receipt);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_observe_write_receipt_file(const struct p101_env *env, struct p101_error *err, const struct report_paths *paths, const struct observe_result *result)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->receipt, "w");
    if(stream == NULL)
    {
        goto done;    // GCOVR_EXCL_LINE -- fopen failure propagation
    }

    p101_fputs(env, err, "p101-observe receipt\n", stream);
    p101_fputs(env, err, "schema=p101-run-receipt-v1\n", stream);
    p101_fprintf(env, err, stream, "run_id=%s\n", paths->run_id);
    p101_fputs(env, err, "event_schema=" P101_TOOL_EVENT_SCHEMA_NAME "\n", stream);
    p101_fprintf(env, err, stream, "event_log_version=%d\n", P101_TOOL_EVENT_LOG_VERSION);
    p101_fputs(env, err, "ordering=per-context-sequence\n", stream);
    p101_fputs(env, err, "durability=buffered-until-close\n", stream);
    p101_fputs(env, err, "fingerprint=fnv1a64\n", stream);
    p101_fputs(env, err, "fingerprint_security=change-detection-only\n", stream);
    p101_fputs(env, err, "analysis=deferred\n", stream);

    write_status(env, err, stream, "command", result->command_status);

    write_artifact_fingerprint(env, err, stream, "manifest", paths->manifest);
    write_artifact_fingerprint(env, err, stream, "command", paths->command);
    write_artifact_fingerprint(env, err, stream, "stdout", paths->stdout_text);
    write_artifact_fingerprint(env, err, stream, "stderr", paths->stderr_text);
    write_artifact_fingerprint(env, err, stream, "resources", paths->resource_log);
    write_artifact_fingerprint(env, err, stream, "calls", paths->call_log);
    write_artifact_fingerprint(env, err, stream, "summary", paths->summary);

    p101_fputs(env, err, "does_not_prove=complete instrumentation, external truth, global process ordering, or cryptographic authenticity\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void write_artifact_fingerprint(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *role, const char *path)
{
    struct p101_tool_event_fingerprint fingerprint;

    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- prior output failure propagation
    {
        return;    // GCOVR_EXCL_LINE
    }
    if(p101_tool_event_fingerprint_file(err, path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint) != 0)
    {
        return;
    }
    p101_fprintf(env, err, stream, "artifact=%s\tbytes=%zu\trecords=%zu\tfinal_newline=%d\tfnv1a64=%016" PRIx64 "\n", role, fingerprint.bytes, fingerprint.records, fingerprint.final_newline, fingerprint.fnv1a64);
}

static void write_status(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *role, int status)
{
    if(WIFEXITED(status))    // GCOVR_EXCL_BR_LINE -- libc macro has platform-specific internal branches
    {
        p101_fprintf(env, err, stream, "status=%s\texit=%d\n", role, WEXITSTATUS(status));
    }
    else if(WIFSIGNALED(status))    // GCOVR_EXCL_BR_LINE -- libc macro has platform-specific internal branches
    {
        p101_fprintf(env, err, stream, "status=%s\tsignal=%d\n", role, WTERMSIG(status));
    }
    else
    {
        p101_fprintf(env, err, stream, "status=%s\traw=%d\n", role, status);
    }
}
