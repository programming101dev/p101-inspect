#include <errno.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <limits.h>
#include <p101_error/error.h>
#include <p101_tool_event/analysis.h>
#include <p101_tool_event/lesson_catalog.h>
#include <p101_tool_event/model.h>
#include <p101_tool_event/receipt.h>
#include <p101_tool_event/report.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
    #include <crt_externs.h>
#endif

#if !defined(__APPLE__) && (!defined(__GLIBC__) || !defined(__USE_MISC))
extern char **environ;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

enum
{
    EXIT_FINDINGS               = 1,
    EXIT_TROUBLE                = 2,
    DECIMAL_BASE                = 10,
    HEXADECIMAL_BASE            = 16,
    MAX_COMMAND_EXIT_STATUS     = 255,
    ARTIFACT_ROLE_SIZE          = 64,
    RECEIPT_IDENTITY_SIZE       = 64,
    EDGE_NAME_SIZE              = 64,
    ANALYSIS_ARGUMENT_COUNT     = 5,
    MODEL_VERIFY_ARGUMENT_COUNT = 5,
    CAPTURE_ARGUMENT_OVERHEAD   = 7,
    INTERLEAVING_SCHEDULE_LIMIT = 256,
    OUTPUT_DIRECTORY_MODE       = 0775,
    PATH_SIZE                   = 4096,
    RECEIPT_LINE_SIZE           = 8192,
    COPY_BUFFER_SIZE            = 8192
};

struct artifact
{
    const char *role;
    const char *name;
    bool        observed;
};

struct loaded_analysis
{
    struct p101_error         *err;
    struct p101_tool_model    *model;
    struct p101_tool_analysis *analysis;
};

struct finding_reference
{
    const struct p101_tool_analysis_finding *finding;
};

struct rule_definition
{
    const char *identifier;
    const char *kind;
    const char *pattern;
    const char *title;
    const char *lesson;
};

struct rule_pack_definition
{
    const char                   *name;
    const struct rule_definition *rules;
    size_t                        rule_count;
};

#include "rule_catalog.inc"

static char capture_program[PATH_SIZE] = "inspect-capture";    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void                               usage(FILE *stream, const char *program);
static int                                analyze_command(int argc, char *const argv[]);
static int                                run_command(int argc, char *argv[]);
static int                                view_command(int argc, char *argv[]);
static int                                model_command(int argc, char *argv[]);
static int                                interleaving_command(int argc, char *argv[]);
static int                                verify_capture(struct p101_error *err, const char *directory, int *command_status);
static int                                verify_artifact(struct p101_error *err, const char *directory, const char *line, struct artifact *artifacts, size_t artifact_count);
static int                                parse_artifact_line(const char *line, char role[ARTIFACT_ROLE_SIZE], size_t *bytes, size_t *records, int *final_newline, uint64_t *hash);
static int                                parse_unsigned_field(const char **cursor, const char *prefix, int base, uintmax_t maximum, uintmax_t *value);
static int                                join_path(char destination[PATH_SIZE], const char *directory, const char *name);
static int                                write_model_file(struct p101_error *err, const struct p101_tool_model *model, const char *directory);
static int                                write_analysis_summary(struct p101_error *err, const struct p101_tool_analysis *analysis, const char *capture, const char *output);
static int                                write_finding_index(struct p101_error *err, const struct p101_tool_analysis *analysis, const char *output);
static int                                copy_analysis_inputs(struct p101_error *err, const char *capture, const char *output);
static int                                write_analysis_manifest(struct p101_error *err, const char *output);
static int                                write_analysis_receipt(struct p101_error *err, const struct p101_tool_analysis *analysis, const char *output);
static int                                spawn_and_wait(char *const argv[]);
static char                             **process_environment(void);
static int                                copy_file_to_stream(const char *path, FILE *destination);
static int                                render_artifact(const char *directory, const char *name, const char *output_path, FILE *default_stream);
static int                                file_contains(const char *path, const char *needle, bool *contains);
static int                                analysis_result(const char *directory, int *status);
static int                                verify_analysis_manifest(struct p101_error *err, const char *directory);
static void                               locate_capture_program(const char *program);
static int                                load_analysis_directory(const char *directory, struct loaded_analysis *loaded);
static void                               unload_analysis(struct loaded_analysis *loaded);
static const char                        *model_edge_name(p101_tool_model_edge_kind kind);
static bool                               pattern_matches(const char *pattern, const char *value);
static int                                model_verify(const char *directory, const char *expectations);
static int                                model_check(const char *directory, int argc, char *const argv[]);
static int                                model_explain(const char *directory, const char *diagnostic_id);
static int                                model_compare(const char *before_directory, const char *after_directory);
static int                                compare_file_pair(const char *before_path, const char *after_path, bool *different);
static bool                               analysis_has_finding(const struct p101_tool_analysis *analysis, const char *pattern);
static const struct rule_pack_definition *find_rule_pack(const char *name);
static bool                               rule_has_evidence(const struct loaded_analysis *loaded, const struct rule_definition *rule);

int main(int argc, char *argv[])
{
    int p101_single_result_;
    int comparison;

    p101_single_result_ = EXIT_TROUBLE;
    locate_capture_program(argv[0]);
    if(argc < 2)
    {
        usage(stderr, argv[0]);
        goto p101_single_exit_;
    }
    comparison = strcmp(argv[1], "analyze");
    if(comparison == 0)
    {
        p101_single_result_ = analyze_command(argc - 1, argv + 1);
    }
    else
    {
        comparison = strcmp(argv[1], "run");
        if(comparison == 0)
        {
            p101_single_result_ = run_command(argc - 1, argv + 1);
        }
        else
        {
            comparison = strcmp(argv[1], "view");
            if(comparison == 0)
            {
                p101_single_result_ = view_command(argc - 1, argv + 1);
            }
            else
            {
                comparison = strcmp(argv[1], "model");
                if(comparison == 0)
                {
                    p101_single_result_ = model_command(argc - 1, argv + 1);
                }
                else
                {
                    comparison = strcmp(argv[1], "interleaving");
                    if(comparison == 0)
                    {
                        p101_single_result_ = interleaving_command(argc - 1, argv + 1);
                    }
                    else
                    {
                        comparison = strcmp(argv[1], "capture");
                        if(comparison == 0)
                        {
                            argv[1]             = capture_program;
                            p101_single_result_ = spawn_and_wait(argv + 1);
                        }
                        else
                        {
                            comparison = strcmp(argv[1], "-h");
                            if(comparison != 0)
                            {
                                comparison = strcmp(argv[1], "--help");
                            }
                            if(comparison == 0)
                            {
                                usage(stdout, argv[0]);
                                p101_single_result_ = EXIT_SUCCESS;
                            }
                            else
                            {
                                usage(stderr, argv[0]);
                            }
                        }
                    }
                }
            }
        }
    }

p101_single_exit_:
    return p101_single_result_;
}

static void usage(FILE *stream, const char *program)
{
    int operation_status;

    operation_status = fprintf(stream,
                               "Usage:\n"
                               "  %s capture [capture-options] -- command [args...]\n"
                               "  %s analyze [-o output] [--force] capture\n"
                               "  %s run [-o output] -- command [args...]\n"
                               "  %s view [-d:human|json|human,json] [-m|-s] [-o file] resource|sync|trace|report analysis\n"
                               "  %s model verify analysis\n"
                               "  %s model compare before after\n"
                               "  %s interleaving analysis\n",
                               program,
                               program,
                               program,
                               program,
                               program,
                               program,
                               program);
    (void)operation_status;
}

// cppcheck-suppress constParameter ; keep the main-compatible argument-vector type.
static int analyze_command(int argc, char *const argv[])
{
    int                        p101_single_result_;
    const char                *capture;
    const char                *output;
    char                       default_output[PATH_SIZE];
    bool                       force;
    struct stat                status;
    struct p101_error         *err;
    struct p101_tool_model    *model;
    struct p101_tool_analysis *analysis;
    char                       resources[PATH_SIZE];
    char                       calls[PATH_SIZE];
    char                       stderr_path[PATH_SIZE];
    int                        operation_status;
    int                        command_status;
    bool                       has_error;
    int                        argument_comparison;

    capture        = NULL;
    output         = NULL;
    force          = false;
    err            = NULL;
    model          = NULL;
    analysis       = NULL;
    command_status = EXIT_SUCCESS;
    for(int index = 1; index < argc; index++)
    {
        argument_comparison = strcmp(argv[index], "-o");
        if(argument_comparison != 0)
        {
            argument_comparison = strcmp(argv[index], "--output");
        }
        if(argument_comparison == 0)
        {
            index++;
            if(index >= argc || output != NULL)
            {
                usage(stderr, "p101-inspect");
                p101_single_result_ = EXIT_TROUBLE;
                goto p101_single_exit_;
            }
            output = argv[index];
        }
        else
        {
            argument_comparison = strcmp(argv[index], "--force");
            if(argument_comparison == 0)
            {
                force = true;
            }
            else if(capture == NULL)
            {
                capture = argv[index];
            }
            else
            {
                usage(stderr, "p101-inspect");
                p101_single_result_ = EXIT_TROUBLE;
                goto p101_single_exit_;
            }
        }
    }
    if(capture == NULL)
    {
        usage(stderr, "p101-inspect");
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    if(output == NULL)
    {
        operation_status = snprintf(default_output, sizeof(default_output), "%s.analysis", capture);
        if(operation_status < 0 || (size_t)operation_status >= sizeof(default_output))
        {
            p101_single_result_ = EXIT_TROUBLE;
            goto p101_single_exit_;
        }
        output = default_output;
    }
    operation_status = lstat(output, &status);
    if(operation_status == 0 || errno != ENOENT)
    {
        fprintf(stderr, "p101-inspect: output path already exists: %s\n", output);
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    err = p101_error_create(false);
    if(err == NULL)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    operation_status = verify_capture(err, capture, &command_status);
    if(operation_status != 0 && !force)
    {
        goto done;
    }
    if(operation_status != 0)
    {
        p101_error_reset(err);
    }
    operation_status = join_path(resources, capture, "resources.log");
    if(operation_status == 0)
    {
        operation_status = join_path(calls, capture, "calls.log");
    }
    if(operation_status == 0)
    {
        operation_status = join_path(stderr_path, capture, "stderr.txt");
    }
    if(operation_status != 0)
    {
        goto done;
    }
    has_error = p101_error_has_error(err);
    if(has_error)
    {
        goto done;
    }
    model     = p101_tool_model_create(err);
    has_error = p101_error_has_error(err);
    if(model == NULL || has_error)
    {
        goto done;
    }
    operation_status = p101_tool_model_ingest_paths(err, model, resources, calls);
    has_error        = p101_error_has_error(err);
    if(operation_status != 0 || has_error)
    {
        goto done;
    }
    analysis  = p101_tool_analysis_create(err, model);
    has_error = p101_error_has_error(err);
    if(analysis == NULL || has_error)
    {
        goto done;
    }
    operation_status = p101_tool_analysis_run(err, analysis, stderr_path);
    has_error        = p101_error_has_error(err);
    if(has_error)
    {
        operation_status = -1;
    }
    if(operation_status == 0)
    {
        operation_status = p101_tool_analysis_write_bundle(err, analysis, output);
    }
    if(operation_status == 0)
    {
        operation_status = write_model_file(err, model, output);
    }
    if(operation_status == 0)
    {
        operation_status = copy_analysis_inputs(err, capture, output);
    }
    if(operation_status == 0)
    {
        operation_status = write_finding_index(err, analysis, output);
    }
    if(operation_status == 0)
    {
        operation_status = write_analysis_summary(err, analysis, capture, output);
    }
    if(operation_status == 0)
    {
        operation_status = write_analysis_manifest(err, output);
    }
    if(operation_status == 0)
    {
        operation_status = write_analysis_receipt(err, analysis, output);
    }
    if(operation_status != 0)
    {
        goto done;
    }
    p101_single_result_ = p101_tool_analysis_status(analysis);
    if(command_status == EXIT_FINDINGS && p101_single_result_ == EXIT_SUCCESS)
    {
        p101_single_result_ = EXIT_FINDINGS;
    }
    fprintf(stdout, "p101-inspect: %s: %s\n", p101_single_result_ == 0 ? "clean" : "findings", output);
    goto cleanup;

done:
    has_error = p101_error_has_error(err);
    if(has_error)
    {
        const char *message;

        message = p101_error_get_message(err);
        fprintf(stderr, "p101-inspect: analysis failed: %s\n", message);
    }
    p101_single_result_ = EXIT_TROUBLE;

cleanup:
    p101_tool_analysis_destroy(&analysis);
    p101_tool_model_destroy(&model);
    p101_error_destroy(err);

p101_single_exit_:
    return p101_single_result_;
}

static int verify_capture(struct p101_error *err, const char *directory, int *command_status)
{
    int             p101_single_result_;
    struct artifact artifacts[] = {
        {"manifest",  "manifest.txt",  false},
        {"command",   "command.txt",   false},
        {"stdout",    "stdout.txt",    false},
        {"stderr",    "stderr.txt",    false},
        {"resources", "resources.log", false},
        {"calls",     "calls.log",     false},
        {"summary",   "summary.txt",   false},
    };
    char        path[PATH_SIZE];
    FILE       *stream;
    char        line[RECEIPT_LINE_SIZE];
    bool        header_valid;
    bool        command_observed;
    int         operation_status;
    const char *read_result;
    int         comparison;
    size_t      artifact_count;

    operation_status = join_path(path, directory, "receipt.txt");
    if(operation_status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    stream = fopen(path, "re");
    if(stream == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    header_valid     = false;
    command_observed = false;
    read_result      = fgets(line, sizeof(line), stream);
    if(read_result != NULL)
    {
        comparison   = strcmp(line, "inspect-capture receipt\n");
        header_valid = comparison == 0;
    }
    if(header_valid)
    {
        operation_status = 0;
    }
    else
    {
        operation_status = -1;
    }
    read_result = NULL;
    if(operation_status == 0)
    {
        line[0]     = '\0';
        read_result = fgets(line, sizeof(line), stream);
    }
    while(operation_status == 0 && read_result != NULL)
    {
        comparison = strncmp(line, "artifact=", sizeof("artifact=") - 1U);
        if(comparison == 0)
        {
            artifact_count   = sizeof(artifacts) / sizeof(artifacts[0]);
            operation_status = verify_artifact(err, directory, line, artifacts, artifact_count);
        }
        else
        {
            comparison = strncmp(line, "status=command\texit=", sizeof("status=command\texit=") - 1U);
            if(comparison == 0)
            {
                char *end;
                long  value;

                errno = 0;
                value = strtol(line + (sizeof("status=command\texit=") - 1U), &end, DECIMAL_BASE);
                if(errno != 0 || (*end != '\n' && *end != '\0') || value < 0 || value > MAX_COMMAND_EXIT_STATUS)
                {
                    operation_status = -1;
                }
                else
                {
                    *command_status  = (int)value == 0 ? EXIT_SUCCESS : EXIT_FINDINGS;
                    command_observed = true;
                }
            }
            else
            {
                comparison = strncmp(line, "status=command\tsignal=", sizeof("status=command\tsignal=") - 1U);
                if(comparison == 0)
                {
                    *command_status  = EXIT_TROUBLE;
                    command_observed = true;
                }
            }
        }
        read_result = fgets(line, sizeof(line), stream);
    }
    comparison = ferror(stream);
    if(comparison != 0)
    {
        operation_status = -1;
    }
    comparison = fclose(stream);
    if(comparison != 0)
    {
        operation_status = -1;
    }
    artifact_count = sizeof(artifacts) / sizeof(artifacts[0]);
    for(size_t index = 0U; index < artifact_count; index++)
    {
        if(!artifacts[index].observed)
        {
            operation_status = -1;
        }
    }
    if(!command_observed)
    {
        operation_status = -1;
    }
    if(operation_status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int verify_artifact(struct p101_error *err, const char *directory, const char *line, struct artifact *artifacts, size_t artifact_count)
{
    int                                p101_single_result_;
    char                               role[ARTIFACT_ROLE_SIZE];
    size_t                             expected_bytes;
    size_t                             expected_records;
    int                                expected_final_newline;
    uint64_t                           expected_hash;
    struct artifact                   *artifact;
    char                               path[PATH_SIZE];
    struct p101_tool_event_fingerprint fingerprint;
    int                                operation_status;

    operation_status = parse_artifact_line(line, role, &expected_bytes, &expected_records, &expected_final_newline, &expected_hash);
    if(operation_status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    artifact = NULL;
    for(size_t index = 0U; index < artifact_count; index++)
    {
        int comparison;

        comparison = strcmp(artifacts[index].role, role);
        if(comparison == 0)
        {
            artifact = &artifacts[index];
            break;
        }
    }
    if(artifact == NULL || artifact->observed)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    operation_status = join_path(path, directory, artifact->name);
    if(operation_status == 0)
    {
        operation_status = p101_tool_event_fingerprint_file(err, path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint);
    }
    if(operation_status != 0 || fingerprint.bytes != expected_bytes || fingerprint.records != expected_records || fingerprint.final_newline != expected_final_newline || fingerprint.fnv1a64 != expected_hash)
    {
        if(operation_status == 0)
        {
            P101_ERROR_RAISE_ERRNO(err, EINVAL);
        }
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    artifact->observed  = true;
    p101_single_result_ = 0;

p101_single_exit_:
    return p101_single_result_;
}

static int parse_artifact_line(const char *line, char role[ARTIFACT_ROLE_SIZE], size_t *bytes, size_t *records, int *final_newline, uint64_t *hash)
{
    int         p101_single_result_;
    const char *cursor;
    const char *separator;
    size_t      role_length;
    uintmax_t   parsed_value;
    int         operation_status;

    cursor           = line;
    operation_status = strncmp(cursor, "artifact=", sizeof("artifact=") - 1U);
    if(operation_status != 0)
    {
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    cursor += sizeof("artifact=") - 1U;
    separator   = strchr(cursor, '\t');
    role_length = separator == NULL ? 0U : (size_t)(separator - cursor);
    if(role_length == 0U || role_length >= ARTIFACT_ROLE_SIZE)
    {
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    memcpy(role, cursor, role_length);
    role[role_length] = '\0';
    cursor            = separator;
    operation_status  = parse_unsigned_field(&cursor, "\tbytes=", DECIMAL_BASE, SIZE_MAX, &parsed_value);
    if(operation_status == 0)
    {
        *bytes           = (size_t)parsed_value;
        operation_status = parse_unsigned_field(&cursor, "\trecords=", DECIMAL_BASE, SIZE_MAX, &parsed_value);
    }
    if(operation_status == 0)
    {
        *records         = (size_t)parsed_value;
        operation_status = parse_unsigned_field(&cursor, "\tfinal_newline=", DECIMAL_BASE, 1U, &parsed_value);
    }
    if(operation_status == 0)
    {
        *final_newline   = (int)parsed_value;
        operation_status = parse_unsigned_field(&cursor, "\tfnv1a64=", HEXADECIMAL_BASE, UINT64_MAX, &parsed_value);
    }
    if(operation_status == 0)
    {
        *hash = (uint64_t)parsed_value;
        if(*cursor != '\n' && *cursor != '\0')
        {
            operation_status = -1;
        }
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int parse_unsigned_field(const char **cursor, const char *prefix, int base, uintmax_t maximum, uintmax_t *value)
{
    int         p101_single_result_;
    size_t      prefix_length;
    int         comparison;
    const char *number;
    char       *end;
    uintmax_t   parsed_value;

    prefix_length = strlen(prefix);
    comparison    = strncmp(*cursor, prefix, prefix_length);
    if(comparison != 0)
    {
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    number       = *cursor + prefix_length;
    errno        = 0;
    parsed_value = strtoumax(number, &end, base);
    if(errno != 0 || end == number || parsed_value > maximum)
    {
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    *cursor             = end;
    *value              = parsed_value;
    p101_single_result_ = 0;

p101_single_exit_:
    return p101_single_result_;
}

static int join_path(char destination[PATH_SIZE], const char *directory, const char *name)
{
    int p101_single_result_;
    int operation_status;

    operation_status    = snprintf(destination, PATH_SIZE, "%s/%s", directory, name);
    p101_single_result_ = operation_status < 0 || (size_t)operation_status >= PATH_SIZE ? -1 : 0;
    return p101_single_result_;
}

static int write_model_file(struct p101_error *err, const struct p101_tool_model *model, const char *directory)
{
    int   p101_single_result_;
    char  path[PATH_SIZE];
    FILE *stream;
    int   operation_status;
    int   close_status;

    operation_status = join_path(path, directory, "run-model.json");
    if(operation_status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    stream = fopen(path, "we");
    if(stream == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    operation_status = p101_tool_model_write_json(err, stream, model);
    close_status     = fclose(stream);
    if(close_status != 0 && operation_status == 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        operation_status = -1;
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int write_analysis_summary(struct p101_error *err, const struct p101_tool_analysis *analysis, const char *capture, const char *output)
{
    int    p101_single_result_;
    char   path[PATH_SIZE];
    FILE  *stream;
    int    operation_status;
    int    close_status;
    int    status;
    size_t findings;

    operation_status = join_path(path, output, "summary.md");
    if(operation_status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    stream = fopen(path, "we");
    if(stream == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    status           = p101_tool_analysis_status(analysis);
    findings         = p101_tool_analysis_finding_count(analysis);
    operation_status = fprintf(stream,
                               "# p101 replay analysis\n\n"
                               "Capture: `%s`\n\n"
                               "Capture verification: **VERIFIED**\n\n"
                               "Findings: %zu\n\n"
                               "Overall result: **%s**\n\n"
                               "This report is bounded by the captured p101 wrapper events. It cannot see direct libc calls, third-party internals, or events that were never emitted.\n",
                               capture,
                               findings,
                               status == 0 ? "CLEAN" : "FINDINGS");
    if(operation_status < 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EIO);
        operation_status = -1;
    }
    else
    {
        operation_status = 0;
    }
    close_status = fclose(stream);
    if(close_status != 0 && operation_status == 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        operation_status = -1;
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int finding_pointer_compare(const void *left, const void *right)
{
    const struct finding_reference *left_reference;
    const struct finding_reference *right_reference;
    int                             p101_single_result_;

    left_reference      = (const struct finding_reference *)left;
    right_reference     = (const struct finding_reference *)right;
    p101_single_result_ = strcmp(left_reference->finding->diagnostic_id, right_reference->finding->diagnostic_id);
    if(p101_single_result_ == 0)
    {
        p101_single_result_ = strcmp(left_reference->finding->file_name, right_reference->finding->file_name);
    }
    if(p101_single_result_ == 0 && left_reference->finding->line_number != right_reference->finding->line_number)
    {
        p101_single_result_ = left_reference->finding->line_number < right_reference->finding->line_number ? -1 : 1;
    }
    if(p101_single_result_ == 0)
    {
        p101_single_result_ = strcmp(left_reference->finding->function_name, right_reference->finding->function_name);
    }
    if(p101_single_result_ == 0)
    {
        p101_single_result_ = strcmp(left_reference->finding->evidence_from, right_reference->finding->evidence_from);
    }
    if(p101_single_result_ == 0)
    {
        p101_single_result_ = strcmp(left_reference->finding->evidence_to, right_reference->finding->evidence_to);
    }
    return p101_single_result_;
}

static int copy_analysis_inputs(struct p101_error *err, const char *capture, const char *output)
{
    static const char *const source_names[]      = {"resources.log", "calls.log", "stderr.txt"};
    static const char *const destination_names[] = {"input-resources.log", "input-calls.log", "input-stderr.txt"};
    int                      p101_single_result_;
    int                      operation_status;
    size_t                   source_count;

    operation_status = 0;
    source_count     = sizeof(source_names) / sizeof(source_names[0]);
    for(size_t index = 0U; index < source_count && operation_status == 0; index++)
    {
        char  source_path[PATH_SIZE];
        char  destination_path[PATH_SIZE];
        FILE *destination;
        int   close_status;

        operation_status = join_path(source_path, capture, source_names[index]);
        if(operation_status == 0)
        {
            operation_status = join_path(destination_path, output, destination_names[index]);
        }
        destination = NULL;
        if(operation_status == 0)
        {
            destination = fopen(destination_path, "we");
        }
        if(destination == NULL)
        {
            P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
            operation_status = -1;
            break;
        }
        operation_status = copy_file_to_stream(source_path, destination);
        close_status     = fclose(destination);
        if(close_status != 0 && operation_status == 0)
        {
            P101_ERROR_RAISE_ERRNO(err, errno);
            operation_status = -1;
        }
    }
    p101_single_result_ = operation_status;
    return p101_single_result_;
}

static int write_finding_index(struct p101_error *err, const struct p101_tool_analysis *analysis, const char *output)
{
    int                       p101_single_result_;
    struct finding_reference *findings;
    size_t                    finding_count;
    char                      path[PATH_SIZE];
    FILE                     *stream;
    int                       operation_status;
    void                     *allocation;
    int                       close_status;

    finding_count = p101_tool_analysis_finding_count(analysis);
    allocation    = calloc(finding_count == 0U ? 1U : finding_count, sizeof(*findings));
    findings      = (struct finding_reference *)allocation;
    if(findings == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? ENOMEM : errno);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    for(size_t index = 0U; index < finding_count; index++)
    {
        findings[index].finding = p101_tool_analysis_finding_at(analysis, index);
    }
    qsort((void *)findings, finding_count, sizeof(*findings), finding_pointer_compare);
    operation_status = join_path(path, output, "finding-index.tsv");
    stream           = NULL;
    if(operation_status == 0)
    {
        stream = fopen(path, "we");
    }
    if(stream == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
        operation_status = -1;
        goto done;
    }
    operation_status = fputs("p101-finding-index-v1\n", stream);
    operation_status = operation_status < 0 ? -1 : 0;
    for(size_t index = 0U; index < finding_count && operation_status == 0; index++)
    {
        const struct p101_tool_analysis_finding *finding;

        finding          = findings[index].finding;
        operation_status = fprintf(stream, "%s\t%s\t%d\t%s\t%s\t%s\n", finding->diagnostic_id, finding->file_name, finding->line_number, finding->function_name, finding->evidence_from, finding->evidence_to);
        operation_status = operation_status < 0 ? -1 : 0;
    }
    close_status = fclose(stream);
    if(close_status != 0 && operation_status == 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        operation_status = -1;
    }

done:
    free((void *)findings);
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int write_analysis_manifest(struct p101_error *err, const char *output)
{
    static const char *const names[] = {
        "resource-report.txt",
        "resource-report.json",
        "concurrency-report.txt",
        "concurrency-report.json",
        "trace-tree.txt",
        "trace-summary.txt",
        "sanitizer-report.txt",
        "sanitizer-report.json",
        "correlated-report.txt",
        "correlated-report.json",
        "resource-lifetimes.md",
        "run-model.json",
        "summary.md",
        "finding-index.tsv",
        "input-resources.log",
        "input-calls.log",
        "input-stderr.txt",
    };
    int    p101_single_result_;
    char   manifest_path[PATH_SIZE];
    FILE  *stream;
    int    operation_status;
    int    close_status;
    size_t name_count;

    operation_status = join_path(manifest_path, output, "analysis-manifest.txt");
    stream           = NULL;
    if(operation_status == 0)
    {
        stream = fopen(manifest_path, "we");
    }
    if(stream == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    operation_status = fputs("p101-analysis-manifest-v1\n", stream);
    name_count       = sizeof(names) / sizeof(names[0]);
    for(size_t index = 0U; index < name_count && operation_status >= 0; index++)
    {
        char                               path[PATH_SIZE];
        struct p101_tool_event_fingerprint fingerprint;

        operation_status = join_path(path, output, names[index]);
        if(operation_status == 0)
        {
            operation_status = p101_tool_event_fingerprint_file(err, path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint);
        }
        if(operation_status == 0)
        {
            operation_status = fprintf(stream, "artifact=%s\tbytes=%zu\trecords=%zu\tfinal_newline=%d\tfnv1a64=%016" PRIx64 "\n", names[index], fingerprint.bytes, fingerprint.records, fingerprint.final_newline, fingerprint.fnv1a64);
            operation_status = operation_status < 0 ? -1 : 0;
        }
    }
    if(operation_status < 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
        operation_status = -1;
    }
    close_status = fclose(stream);
    if(close_status != 0 && operation_status == 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        operation_status = -1;
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int write_analysis_receipt(struct p101_error *err, const struct p101_tool_analysis *analysis, const char *output)
{
    int                                      p101_single_result_;
    char                                     input_path[PATH_SIZE];
    char                                     output_path[PATH_SIZE];
    char                                     identity[RECEIPT_IDENTITY_SIZE];
    struct p101_tool_event_fingerprint       fingerprint;
    struct p101_tool_run_receipt             receipt;
    FILE                                    *stream;
    int                                      operation_status;
    int                                      close_status;
    int                                      analysis_status;
    size_t                                   finding_count;
    const struct p101_tool_analysis_finding *first_finding;

    operation_status = join_path(input_path, output, "analysis-manifest.txt");
    if(operation_status == 0)
    {
        operation_status = join_path(output_path, output, "analysis-receipt.json");
    }
    if(operation_status == 0)
    {
        operation_status = p101_tool_event_fingerprint_file(err, input_path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint);
    }
    if(operation_status != 0)
    {
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    operation_status = snprintf(identity, sizeof(identity), "fnv1a64:%016" PRIx64, fingerprint.fnv1a64);
    if(operation_status < 0 || (size_t)operation_status >= sizeof(identity))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    analysis_status = p101_tool_analysis_status(analysis);
    finding_count   = p101_tool_analysis_finding_count(analysis);
    memset(&receipt, 0, sizeof(receipt));
    receipt.tool_name       = "p101-inspect";
    receipt.tool_version    = "3.0.0";
    receipt.input_schema    = "p101-run-receipt-v1";
    receipt.input_identity  = identity;
    receipt.policy_schema   = "p101-runtime-policy-v2";
    receipt.policy_identity = "native-causal-model";
    receipt.run_identity    = identity;
    receipt.outcome         = analysis_status == 0 ? P101_TOOL_OUTCOME_CLEAN : P101_TOOL_OUTCOME_FINDINGS;
    receipt.failure_reason  = analysis_status == 0 ? P101_TOOL_FAILURE_NONE : P101_TOOL_FAILURE_FINDINGS_PRESENT;
    receipt.failed_stage    = analysis_status == 0 ? "" : "runtime-policy";
    first_finding           = NULL;
    if(finding_count != 0U)
    {
        first_finding = p101_tool_analysis_finding_at(analysis, 0U);
    }
    receipt.first_diagnostic = first_finding == NULL ? "" : first_finding->diagnostic_id;
    receipt.checks_attempted = 4U;
    receipt.checks_completed = 4U;
    receipt.does_not_prove   = "complete instrumentation, external truth, global process ordering, or cryptographic authenticity";
    stream                   = fopen(output_path, "we");
    if(stream == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    operation_status = p101_tool_run_receipt_write_json(err, stream, &receipt, &fingerprint);
    close_status     = fclose(stream);
    if(close_status != 0 && operation_status == 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno);
        operation_status = -1;
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static int run_command(int argc, char *argv[])
{
    int         p101_single_result_;
    const char *output;
    int         command_index;
    struct stat status;
    char        capture[PATH_SIZE];
    char        analysis[PATH_SIZE];
    char      **capture_argv;
    size_t      capture_argc;
    int         operation_status;
    int         capture_status;
    char       *analysis_argv[ANALYSIS_ARGUMENT_COUNT];
    char       *observe_tool;
    bool        log_arguments;
    bool        log_results;
    int         comparison;
    void       *allocation;
    char        log_arguments_option[] = "-A";
    char        log_results_option[]   = "-R";
    char        output_option[]        = "-o";
    char        option_terminator[]    = "--";
    char        analyze_command_name[] = "analyze";

    output        = NULL;
    command_index = -1;
    observe_tool  = capture_program;
    log_arguments = false;
    log_results   = false;
    for(int index = 1; index < argc; index++)
    {
        comparison = strcmp(argv[index], "-o");
        if(comparison != 0)
        {
            comparison = strcmp(argv[index], "--output");
        }
        if(comparison == 0)
        {
            index++;
            if(index >= argc || output != NULL)
            {
                p101_single_result_ = EXIT_TROUBLE;
                goto p101_single_exit_;
            }
            output = argv[index];
        }
        else
        {
            comparison = strcmp(argv[index], "-A");
            if(comparison != 0)
            {
                comparison = strcmp(argv[index], "--log-arguments");
            }
            if(comparison == 0)
            {
                log_arguments = true;
            }
            else
            {
                comparison = strcmp(argv[index], "-R");
                if(comparison != 0)
                {
                    comparison = strcmp(argv[index], "--log-results");
                }
                if(comparison == 0)
                {
                    log_results = true;
                }
                else
                {
                    comparison = strcmp(argv[index], "--observe-tool");
                    if(comparison == 0)
                    {
                        index++;
                        if(index >= argc)
                        {
                            p101_single_result_ = EXIT_TROUBLE;
                            goto p101_single_exit_;
                        }
                        observe_tool = argv[index];
                    }
                    else
                    {
                        comparison = strcmp(argv[index], "--analyze-tool");
                        if(comparison != 0)
                        {
                            comparison = strcmp(argv[index], "--model-tool");
                        }
                        if(comparison == 0)
                        {
                            index++;
                            if(index >= argc)
                            {
                                p101_single_result_ = EXIT_TROUBLE;
                                goto p101_single_exit_;
                            }
                        }
                        else
                        {
                            comparison = strcmp(argv[index], "--");
                            if(comparison == 0)
                            {
                                command_index = index + 1;
                                break;
                            }
                            p101_single_result_ = EXIT_TROUBLE;
                            goto p101_single_exit_;
                        }
                    }
                }
            }
        }
    }
    if(output == NULL || command_index < 0 || command_index >= argc)
    {
        usage(stderr, "p101-inspect");
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    operation_status = lstat(output, &status);
    if(operation_status == 0 || errno != ENOENT)
    {
        fprintf(stderr, "p101-inspect: output path already exists: %s\n", output);
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    operation_status = mkdir(output, OUTPUT_DIRECTORY_MODE);
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    operation_status = join_path(capture, output, "capture");
    if(operation_status == 0)
    {
        operation_status = join_path(analysis, output, "analysis");
    }
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    capture_argc = (size_t)(argc - command_index) + CAPTURE_ARGUMENT_OVERHEAD;
    allocation   = calloc(capture_argc, sizeof(*capture_argv));
    capture_argv = (char **)allocation;
    if(capture_argv == NULL)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    capture_argv[0] = observe_tool;
    capture_argc    = 1U;
    if(log_arguments)
    {
        capture_argv[capture_argc] = log_arguments_option;
        capture_argc++;
    }
    if(log_results)
    {
        capture_argv[capture_argc] = log_results_option;
        capture_argc++;
    }
    capture_argv[capture_argc] = output_option;
    capture_argc++;
    capture_argv[capture_argc] = capture;
    capture_argc++;
    capture_argv[capture_argc] = option_terminator;
    capture_argc++;
    for(int index = command_index; index < argc; index++)
    {
        capture_argv[capture_argc] = argv[index];
        capture_argc++;
    }
    capture_status = spawn_and_wait(capture_argv);
    free((void *)capture_argv);
    if(capture_status == EXIT_TROUBLE)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    analysis_argv[0]    = analyze_command_name;
    analysis_argv[1]    = output_option;
    analysis_argv[2]    = analysis;
    analysis_argv[3]    = capture;
    analysis_argv[4]    = NULL;
    p101_single_result_ = analyze_command(4, analysis_argv);
    if(capture_status == EXIT_FINDINGS && p101_single_result_ == EXIT_SUCCESS)
    {
        p101_single_result_ = EXIT_FINDINGS;
    }

p101_single_exit_:
    return p101_single_result_;
}

static int view_command(int argc, char *argv[])
{
    int                       p101_single_result_;
    const char               *human_name;
    const char               *json_name;
    const char               *output_path;
    const char               *view;
    const char               *directory;
    int                       operation_status;
    int                       comparison;
    p101_tool_analysis_policy policy;
    bool                      overall;
    bool                      mermaid;
    bool                      summary;
    unsigned int              outputs;
    struct loaded_analysis    loaded;
    int                       positional_count;

    human_name       = NULL;
    json_name        = NULL;
    output_path      = NULL;
    view             = NULL;
    directory        = NULL;
    overall          = false;
    mermaid          = false;
    summary          = false;
    outputs          = P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN;
    positional_count = 0;
    operation_status = 0;
    policy           = P101_TOOL_ANALYSIS_RESOURCE;
    for(int index = 1; index < argc && operation_status == 0; index++)
    {
        comparison = strncmp(argv[index], "-d:", 3U);
        if(comparison == 0)
        {
            operation_status = p101_tool_report_parse_output_option(argv[index], &outputs);
        }
        else
        {
            comparison = strcmp(argv[index], "-m");
            if(comparison != 0)
            {
                comparison = strcmp(argv[index], "--mermaid");
            }
            if(comparison == 0)
            {
                mermaid = true;
            }
            else
            {
                comparison = strcmp(argv[index], "-s");
                if(comparison != 0)
                {
                    comparison = strcmp(argv[index], "--summary");
                }
                if(comparison == 0)
                {
                    summary = true;
                }
                else
                {
                    comparison = strcmp(argv[index], "-o");
                    if(comparison != 0)
                    {
                        comparison = strcmp(argv[index], "--output");
                    }
                    if(comparison == 0)
                    {
                        index++;
                        if(index >= argc || output_path != NULL)
                        {
                            operation_status = -1;
                        }
                        else
                        {
                            output_path = argv[index];
                        }
                    }
                    else if(positional_count == 0)
                    {
                        view = argv[index];
                        positional_count++;
                    }
                    else if(positional_count == 1)
                    {
                        directory = argv[index];
                        positional_count++;
                    }
                    else
                    {
                        operation_status = -1;
                    }
                }
            }
        }
    }
    if(operation_status != 0 || positional_count != 2 || (output_path != NULL && outputs == (P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN | P101_TOOL_DIAGNOSTIC_OUTPUT_JSON)))
    {
        usage(stderr, "p101-inspect");
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    comparison = strcmp(view, "resource");
    if(comparison == 0)
    {
        human_name = "resource-report.txt";
        json_name  = "resource-report.json";
        policy     = P101_TOOL_ANALYSIS_RESOURCE;
    }
    else
    {
        comparison = strcmp(view, "sync");
        if(comparison == 0)
        {
            human_name = "concurrency-report.txt";
            json_name  = "concurrency-report.json";
            policy     = P101_TOOL_ANALYSIS_SYNCHRONIZATION;
        }
        else
        {
            comparison = strcmp(view, "trace");
            if(comparison == 0)
            {
                if(summary)
                {
                    human_name = "trace-summary.txt";
                }
                else
                {
                    human_name = "trace-tree.txt";
                }
                json_name = NULL;
                policy    = P101_TOOL_ANALYSIS_TRACE;
            }
            else
            {
                comparison = strcmp(view, "report");
                if(comparison == 0)
                {
                    if(mermaid)
                    {
                        human_name = "resource-lifetimes.md";
                    }
                    else if(summary)
                    {
                        human_name = "summary.md";
                    }
                    else
                    {
                        human_name = "correlated-report.txt";
                    }
                    json_name = "correlated-report.json";
                    policy    = P101_TOOL_ANALYSIS_RESOURCE;
                    overall   = true;
                }
            }
        }
    }
    if(human_name == NULL || (json_name == NULL && (outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_JSON) != 0U) || (mermaid && !overall) || (summary && !overall && policy != P101_TOOL_ANALYSIS_TRACE))
    {
        usage(stderr, "p101-inspect");
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    if((outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN) != 0U)
    {
        FILE *destination;

        destination      = outputs == P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN ? stdout : stderr;
        operation_status = render_artifact(directory, human_name, output_path, destination);
    }
    if(operation_status == 0 && (outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_JSON) != 0U)
    {
        operation_status = render_artifact(directory, json_name, output_path, stdout);
    }
    if(operation_status == 0)
    {
        operation_status = load_analysis_directory(directory, &loaded);
    }
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    if(overall)
    {
        p101_single_result_ = p101_tool_analysis_status(loaded.analysis);
    }
    else
    {
        p101_single_result_ = p101_tool_analysis_policy_status(loaded.analysis, policy);
    }
    unload_analysis(&loaded);

p101_single_exit_:
    return p101_single_result_;
}

static int model_command(int argc, char *argv[])
{
    int p101_single_result_;
    int comparison;

    if(argc < 2)
    {
        usage(stderr, "p101-inspect");
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    comparison = strcmp(argv[1], "verify");
    if((argc == 3 || argc == MODEL_VERIFY_ARGUMENT_COUNT) && comparison == 0)
    {
        const char *directory;
        const char *expectations;

        directory    = argc == 3 ? argv[2] : argv[4];
        expectations = argc == 3 ? NULL : argv[3];
        comparison   = 0;
        if(argc == MODEL_VERIFY_ARGUMENT_COUNT)
        {
            comparison = strcmp(argv[2], "-e");
            if(comparison != 0)
            {
                comparison = strcmp(argv[2], "--expect");
            }
        }
        if(argc == MODEL_VERIFY_ARGUMENT_COUNT && comparison != 0)
        {
            p101_single_result_ = EXIT_TROUBLE;
        }
        else
        {
            p101_single_result_ = model_verify(directory, expectations);
        }
    }
    else
    {
        comparison = strcmp(argv[1], "check");
        if(argc >= MODEL_VERIFY_ARGUMENT_COUNT && comparison == 0)
        {
            p101_single_result_ = model_check(argv[2], argc - 3, argv + 3);
        }
        else
        {
            comparison = strcmp(argv[1], "explain");
            if(argc == 4 && comparison == 0)
            {
                p101_single_result_ = model_explain(argv[2], argv[3]);
            }
            else
            {
                comparison = strcmp(argv[1], "compare");
                if(argc == 4 && comparison == 0)
                {
                    p101_single_result_ = model_compare(argv[2], argv[3]);
                }
                else
                {
                    usage(stderr, "p101-inspect");
                    p101_single_result_ = EXIT_TROUBLE;
                }
            }
        }
    }
p101_single_exit_:
    return p101_single_result_;
}

static int interleaving_command(int argc, char *argv[])
{
    int                    p101_single_result_;
    struct loaded_analysis loaded;
    int                    operation_status;
    size_t                 counterexamples;
    struct p101_error     *err;

    if(argc != 2)
    {
        usage(stderr, "p101-inspect");
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    operation_status = load_analysis_directory(argv[1], &loaded);
    counterexamples  = 0U;
    if(operation_status == 0)
    {
        err              = loaded.err;
        operation_status = p101_tool_analysis_write_interleaving(err, loaded.analysis, stdout, INTERLEAVING_SCHEDULE_LIMIT, &counterexamples);
    }
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
    }
    else
    {
        p101_single_result_ = counterexamples == 0U ? EXIT_SUCCESS : EXIT_FINDINGS;
    }
    unload_analysis(&loaded);

p101_single_exit_:
    return p101_single_result_;
}

static char **process_environment(void)
{
    char **p101_single_result_;

#ifdef __APPLE__
    char ***environment_address;

    environment_address = _NSGetEnviron();
    p101_single_result_ = *environment_address;
#else
    p101_single_result_ = environ;
#endif
    return p101_single_result_;
}

static int spawn_and_wait(char *const argv[])
{
    int    p101_single_result_;
    pid_t  child;
    int    status;
    int    wait_status;
    char **environment;

    environment = process_environment();
    wait_status = posix_spawnp(&child, argv[0], NULL, NULL, argv, environment);
    if(wait_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    do
    {
        wait_status = waitpid(child, &status, 0);
    } while(wait_status < 0 && errno == EINTR);
    if(wait_status < 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
    }
    else
    {
        wait_status = WIFEXITED(status);
        if(wait_status == 0)
        {
            p101_single_result_ = EXIT_TROUBLE;
        }
        else
        {
            p101_single_result_ = WEXITSTATUS(status);
        }
    }

p101_single_exit_:
    return p101_single_result_;
}

static int copy_file_to_stream(const char *path, FILE *destination)
{
    int   p101_single_result_;
    FILE *source;
    int   operation_status;

    source = fopen(path, "re");
    if(source == NULL)
    {
        operation_status = -1;
    }
    else
    {
        char   buffer[COPY_BUFFER_SIZE];
        size_t amount;
        int    read_error;
        int    close_status;

        operation_status = 0;
        amount           = fread(buffer, 1U, sizeof(buffer), source);
        while(operation_status == 0 && amount > 0U)
        {
            size_t written;

            written = fwrite(buffer, 1U, amount, destination);
            if(written != amount)
            {
                operation_status = -1;
            }
            if(operation_status == 0 && amount == sizeof(buffer))
            {
                amount = fread(buffer, 1U, sizeof(buffer), source);
            }
            else
            {
                amount = 0U;
            }
        }
        read_error = ferror(source);
        if(read_error != 0)
        {
            operation_status = -1;
        }
        close_status = fclose(source);
        if(close_status != 0)
        {
            operation_status = -1;
        }
    }
    p101_single_result_ = operation_status;
    return p101_single_result_;
}

static int render_artifact(const char *directory, const char *name, const char *output_path, FILE *default_stream)
{
    int   p101_single_result_;
    char  path[PATH_SIZE];
    FILE *destination;
    int   operation_status;

    operation_status = join_path(path, directory, name);
    destination      = default_stream;
    if(operation_status == 0 && output_path != NULL)
    {
        destination = fopen(output_path, "we");
        if(destination == NULL)
        {
            operation_status = -1;
        }
    }
    if(operation_status == 0)
    {
        operation_status = copy_file_to_stream(path, destination);
    }
    if(output_path != NULL && destination != NULL)
    {
        int close_status;

        close_status = fclose(destination);
        if(close_status != 0)
        {
            operation_status = -1;
        }
    }
    p101_single_result_ = operation_status;
    return p101_single_result_;
}

static int file_contains(const char *path, const char *needle, bool *contains)
{
    int         p101_single_result_;
    FILE       *stream;
    char        line[RECEIPT_LINE_SIZE];
    int         operation_status;
    const char *read_result;

    *contains        = false;
    stream           = fopen(path, "re");
    operation_status = stream == NULL ? -1 : 0;
    read_result      = NULL;
    if(operation_status == 0)
    {
        read_result = fgets(line, sizeof(line), stream);
    }
    while(operation_status == 0 && read_result != NULL)
    {
        const char *match;

        match = strstr(line, needle);
        if(match != NULL)
        {
            *contains = true;
            break;
        }
        read_result = fgets(line, sizeof(line), stream);
    }
    if(stream != NULL)
    {
        int io_status;

        io_status = ferror(stream);
        if(io_status != 0)
        {
            operation_status = -1;
        }
        io_status = fclose(stream);
        if(io_status != 0)
        {
            operation_status = -1;
        }
    }
    p101_single_result_ = operation_status;
    return p101_single_result_;
}

static int analysis_result(const char *directory, int *status)
{
    int                                     p101_single_result_;
    char                                    receipt_path[PATH_SIZE];
    char                                    model_path[PATH_SIZE];
    char                                    manifest_path[PATH_SIZE];
    struct p101_error                      *err;
    struct p101_tool_run_receipt_validation validation;
    struct p101_tool_event_fingerprint      fingerprint;
    bool                                    schema_present;
    int                                     operation_status;

    schema_present   = false;
    err              = p101_error_create(false);
    operation_status = err == NULL ? -1 : 0;
    if(operation_status == 0)
    {
        operation_status = join_path(receipt_path, directory, "analysis-receipt.json");
    }
    if(operation_status == 0)
    {
        operation_status = join_path(model_path, directory, "run-model.json");
    }
    if(operation_status == 0)
    {
        operation_status = join_path(manifest_path, directory, "analysis-manifest.txt");
    }
    if(operation_status == 0)
    {
        operation_status = p101_tool_run_receipt_validate_file(err, receipt_path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, &validation);
    }
    if(operation_status == 0)
    {
        operation_status = validation.status == P101_TOOL_RECEIPT_VALID && validation.fingerprint_present != 0 ? 0 : -1;
    }
    if(operation_status == 0)
    {
        operation_status = p101_tool_event_fingerprint_file(err, manifest_path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint);
    }
    if(operation_status == 0)
    {
        bool fingerprint_matches;

        fingerprint_matches = (_Bool)((fingerprint.bytes == validation.fingerprint.bytes && fingerprint.records == validation.fingerprint.records && fingerprint.final_newline == validation.fingerprint.final_newline &&
                                       fingerprint.fnv1a64 == validation.fingerprint.fnv1a64) != 0);
        if(fingerprint_matches)
        {
            operation_status = 0;
        }
        else
        {
            operation_status = -1;
        }
    }
    if(operation_status == 0)
    {
        operation_status = verify_analysis_manifest(err, directory);
    }
    if(operation_status == 0)
    {
        operation_status = file_contains(model_path, "\"schema\": \"p101-run-model-v1\"", &schema_present);
    }
    if(operation_status == 0 && !schema_present)
    {
        operation_status = -1;
    }
    if(operation_status == 0)
    {
        *status = p101_tool_outcome_exit_status(validation.outcome);
    }
    p101_error_destroy(err);
    p101_single_result_ = operation_status;
    return p101_single_result_;
}

static int verify_analysis_manifest(struct p101_error *err, const char *directory)
{
    static const char *const names[] = {
        "resource-report.txt",
        "resource-report.json",
        "concurrency-report.txt",
        "concurrency-report.json",
        "trace-tree.txt",
        "trace-summary.txt",
        "sanitizer-report.txt",
        "sanitizer-report.json",
        "correlated-report.txt",
        "correlated-report.json",
        "resource-lifetimes.md",
        "run-model.json",
        "summary.md",
        "finding-index.tsv",
        "input-resources.log",
        "input-calls.log",
        "input-stderr.txt",
    };
    int             p101_single_result_;
    struct artifact artifacts[sizeof(names) / sizeof(names[0])];
    char            path[PATH_SIZE];
    FILE           *stream;
    char            line[RECEIPT_LINE_SIZE];
    int             operation_status;
    size_t          artifact_count;
    const char     *read_result;
    int             comparison;
    int             io_status;

    artifact_count = sizeof(names) / sizeof(names[0]);
    for(size_t index = 0U; index < artifact_count; index++)
    {
        artifacts[index].role     = names[index];
        artifacts[index].name     = names[index];
        artifacts[index].observed = false;
    }
    operation_status = join_path(path, directory, "analysis-manifest.txt");
    stream           = NULL;
    if(operation_status == 0)
    {
        stream = fopen(path, "re");
    }
    if(stream == NULL)
    {
        p101_single_result_ = -1;
        goto p101_single_exit_;
    }
    read_result = fgets(line, sizeof(line), stream);
    comparison  = -1;
    if(read_result != NULL)
    {
        comparison = strcmp(line, "p101-analysis-manifest-v1\n");
    }
    operation_status = comparison == 0 ? 0 : -1;
    read_result      = fgets(line, sizeof(line), stream);
    while(operation_status == 0 && read_result != NULL)
    {
        operation_status = verify_artifact(err, directory, line, artifacts, artifact_count);
        read_result      = NULL;
        if(operation_status == 0)
        {
            read_result = fgets(line, sizeof(line), stream);
        }
    }
    io_status = ferror(stream);
    if(io_status != 0)
    {
        operation_status = -1;
    }
    io_status = fclose(stream);
    if(io_status != 0)
    {
        operation_status = -1;
    }
    for(size_t index = 0U; index < artifact_count && operation_status == 0; index++)
    {
        if(!artifacts[index].observed)
        {
            operation_status = -1;
        }
    }
    if(operation_status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
    }
    p101_single_result_ = operation_status;

p101_single_exit_:
    return p101_single_result_;
}

static void locate_capture_program(const char *program)
{
    const char *separator;
    size_t      directory_length;
    int         operation_status;

    separator = strrchr(program, '/');
    if(separator == NULL)
    {
        goto p101_single_exit_;
    }
    directory_length = (size_t)(separator - program);
    operation_status = snprintf(capture_program, sizeof(capture_program), "%.*s/inspect-capture", (int)directory_length, program);
    if(operation_status < 0 || (size_t)operation_status >= sizeof(capture_program))
    {
        operation_status = snprintf(capture_program, sizeof(capture_program), "%s", "inspect-capture");
        (void)operation_status;
    }

p101_single_exit_:
    return;
}

static int load_analysis_directory(const char *directory, struct loaded_analysis *loaded)
{
    int  p101_single_result_;
    char resources[PATH_SIZE];
    char calls[PATH_SIZE];
    char stderr_path[PATH_SIZE];
    int  operation_status;

    memset(loaded, 0, sizeof(*loaded));
    loaded->err      = p101_error_create(false);
    operation_status = loaded->err == NULL ? -1 : 0;
    if(operation_status == 0)
    {
        operation_status = join_path(resources, directory, "input-resources.log");
    }
    if(operation_status == 0)
    {
        operation_status = join_path(calls, directory, "input-calls.log");
    }
    if(operation_status == 0)
    {
        operation_status = join_path(stderr_path, directory, "input-stderr.txt");
    }
    if(operation_status == 0)
    {
        loaded->model    = p101_tool_model_create(loaded->err);
        operation_status = loaded->model == NULL ? -1 : 0;
    }
    if(operation_status == 0)
    {
        operation_status = p101_tool_model_ingest_paths(loaded->err, loaded->model, resources, calls);
    }
    if(operation_status == 0)
    {
        loaded->analysis = p101_tool_analysis_create(loaded->err, loaded->model);
        operation_status = loaded->analysis == NULL ? -1 : 0;
    }
    if(operation_status == 0)
    {
        operation_status = p101_tool_analysis_run(loaded->err, loaded->analysis, stderr_path);
    }
    if(operation_status != 0)
    {
        unload_analysis(loaded);
    }
    p101_single_result_ = operation_status;
    return p101_single_result_;
}

static void unload_analysis(struct loaded_analysis *loaded)
{
    p101_tool_analysis_destroy(&loaded->analysis);
    p101_tool_model_destroy(&loaded->model);
    p101_error_destroy(loaded->err);
    memset(loaded, 0, sizeof(*loaded));
}

static const char *model_edge_name(p101_tool_model_edge_kind kind)
{
    const char *p101_single_result_;

    if(kind == P101_TOOL_MODEL_EDGE_CALL_PARENT)
    {
        p101_single_result_ = "call-parent";
    }
    else if(kind == P101_TOOL_MODEL_EDGE_CALL_RETURN)
    {
        p101_single_result_ = "call-return";
    }
    else if(kind == P101_TOOL_MODEL_EDGE_CALL_CAUSED_EVENT)
    {
        p101_single_result_ = "call-caused-event";
    }
    else if(kind == P101_TOOL_MODEL_EDGE_RESOURCE_LIFETIME)
    {
        p101_single_result_ = "resource-lifetime";
    }
    else if(kind == P101_TOOL_MODEL_EDGE_PROCESS_CHILD_EVENT)
    {
        p101_single_result_ = "process-child-event";
    }
    else
    {
        p101_single_result_ = "unknown";
    }
    return p101_single_result_;
}

static bool pattern_matches(const char *pattern, const char *value)
{
    bool p101_single_result_;
    int  match_status;

    match_status        = fnmatch(pattern, value, 0);
    p101_single_result_ = match_status == 0;
    return p101_single_result_;
}

static bool analysis_has_finding(const struct p101_tool_analysis *analysis, const char *pattern)
{
    bool   p101_single_result_;
    size_t finding_count;

    p101_single_result_ = false;
    finding_count       = p101_tool_analysis_finding_count(analysis);
    for(size_t index = 0U; index < finding_count; index++)
    {
        const struct p101_tool_analysis_finding *finding;
        bool                                     matches;

        finding = p101_tool_analysis_finding_at(analysis, index);
        matches = pattern_matches(pattern, finding->diagnostic_id);
        if(matches)
        {
            p101_single_result_ = true;
            break;
        }
    }
    return p101_single_result_;
}

static const struct rule_pack_definition *find_rule_pack(const char *name)
{
    const struct rule_pack_definition *p101_single_result_;
    size_t                             pack_count;

    p101_single_result_ = NULL;
    pack_count          = sizeof(rule_packs) / sizeof(rule_packs[0]);
    for(size_t index = 0U; index < pack_count; index++)
    {
        int comparison;

        comparison = strcmp(rule_packs[index].name, name);
        if(comparison == 0)
        {
            p101_single_result_ = &rule_packs[index];
            break;
        }
    }
    return p101_single_result_;
}

static bool rule_has_evidence(const struct loaded_analysis *loaded, const struct rule_definition *rule)
{
    bool p101_single_result_;
    int  comparison;

    p101_single_result_ = false;
    comparison          = strcmp(rule->kind, "forbid-finding");
    if(comparison != 0)
    {
        comparison = strcmp(rule->kind, "require-finding");
    }
    if(comparison == 0)
    {
        p101_single_result_ = analysis_has_finding(loaded->analysis, rule->pattern);
        goto p101_single_exit_;
    }
    comparison = strcmp(rule->kind, "forbid-call");
    if(comparison != 0)
    {
        comparison = strcmp(rule->kind, "require-call");
    }
    if(comparison == 0)
    {
        size_t node_count;

        node_count = p101_tool_model_node_count(loaded->model);
        for(size_t index = 0U; index < node_count; index++)
        {
            const struct p101_tool_model_node *node;
            bool                               matches;

            node = p101_tool_model_node_at(loaded->model, index);
            if(node->domain != P101_TOOL_MODEL_NODE_CALL || node->call_kind != P101_TOOL_EVENT_CALL_ENTER || node->call_name == NULL)
            {
                continue;
            }
            matches = pattern_matches(rule->pattern, node->call_name);
            if(matches)
            {
                p101_single_result_ = true;
                break;
            }
        }
        goto p101_single_exit_;
    }
    comparison = strcmp(rule->kind, "require-edge");
    if(comparison == 0)
    {
        size_t edge_count;

        edge_count = p101_tool_model_edge_count(loaded->model);
        for(size_t index = 0U; index < edge_count; index++)
        {
            const struct p101_tool_model_edge *edge;
            const char                        *name;
            bool                               matches;

            edge    = p101_tool_model_edge_at(loaded->model, index);
            name    = model_edge_name(edge->kind);
            matches = pattern_matches(rule->pattern, name);
            if(matches)
            {
                p101_single_result_ = true;
                break;
            }
        }
        goto p101_single_exit_;
    }
    comparison = strcmp(rule->kind, "require-resource");
    if(comparison == 0)
    {
        size_t node_count;

        node_count = p101_tool_model_node_count(loaded->model);
        for(size_t index = 0U; index < node_count; index++)
        {
            const struct p101_tool_model_node *node;
            bool                               matches;

            node = p101_tool_model_node_at(loaded->model, index);
            if(node->domain != P101_TOOL_MODEL_NODE_RESOURCE || node->resource_class == NULL)
            {
                continue;
            }
            matches = pattern_matches(rule->pattern, node->resource_class);
            if(matches)
            {
                p101_single_result_ = true;
                break;
            }
        }
    }

p101_single_exit_:
    return p101_single_result_;
}

static int model_verify(const char *directory, const char *expectations)
{
    int                    p101_single_result_;
    int                    receipt_status;
    struct loaded_analysis loaded;
    int                    operation_status;
    size_t                 failures;
    int                    comparison;

    receipt_status   = EXIT_TROUBLE;
    operation_status = analysis_result(directory, &receipt_status);
    if(operation_status != 0 || receipt_status == EXIT_TROUBLE)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    operation_status = load_analysis_directory(directory, &loaded);
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    failures = 0U;
    if(expectations != NULL)
    {
        FILE       *stream;
        char        line[RECEIPT_LINE_SIZE];
        size_t      line_number;
        const char *read_result;
        int         io_status;

        stream = fopen(expectations, "re");
        if(stream == NULL)
        {
            unload_analysis(&loaded);
            p101_single_result_ = EXIT_TROUBLE;
            goto p101_single_exit_;
        }
        read_result = fgets(line, sizeof(line), stream);
        comparison  = -1;
        if(read_result != NULL)
        {
            comparison = strcmp(line, "p101-expectations-v1\n");
        }
        if(comparison == 0)
        {
            operation_status = 0;
        }
        else
        {
            operation_status = -1;
        }
        line_number = 1U;
        read_result = NULL;
        if(operation_status == 0)
        {
            read_result = fgets(line, sizeof(line), stream);
        }
        while(operation_status == 0 && read_result != NULL)
        {
            char       *separator;
            const char *key;
            const char *value;
            char       *newline;
            bool        satisfied;

            line_number++;
            newline = strchr(line, '\n');
            if(newline != NULL)
            {
                *newline = '\0';
            }
            if(read_result[0] == '\0' || read_result[0] == '#')
            {
                goto next_expectation;
            }
            separator = strchr(line, '=');
            if(separator == NULL)
            {
                operation_status = -1;
                break;
            }
            *separator = '\0';
            key        = line;
            value      = separator + 1;
            comparison = strcmp(key, "result");
            if(comparison == 0)
            {
                const char *actual;

                actual     = receipt_status == EXIT_SUCCESS ? "clean" : "findings";
                comparison = strcmp(actual, value);
                satisfied  = comparison == 0;
            }
            else
            {
                bool count_expectation;

                comparison        = strcmp(key, "finding_count");
                count_expectation = comparison == 0;
                if(!count_expectation)
                {
                    comparison = strcmp(key, "min_nodes");
                }
                if(count_expectation || comparison == 0)
                {
                    char         *end;
                    unsigned long expected;
                    size_t        actual;

                    errno    = 0;
                    expected = strtoul(value, &end, DECIMAL_BASE);
                    if(count_expectation)
                    {
                        actual = p101_tool_analysis_finding_count(loaded.analysis);
                    }
                    else
                    {
                        actual = p101_tool_model_node_count(loaded.model);
                    }
                    if(count_expectation)
                    {
                        satisfied = (_Bool)(errno == 0 && *end == '\0' && actual == expected);
                    }
                    else
                    {
                        satisfied = (_Bool)(errno == 0 && *end == '\0' && actual >= expected);
                    }
                    goto expectation_checked;
                }
                comparison = strcmp(key, "forbid");
                if(comparison == 0)
                {
                    bool present;

                    present   = analysis_has_finding(loaded.analysis, value);
                    satisfied = (_Bool)(!present);
                    goto expectation_checked;
                }
                comparison = strcmp(key, "require");
                if(comparison == 0)
                {
                    satisfied = analysis_has_finding(loaded.analysis, value);
                    goto expectation_checked;
                }
                comparison = strcmp(key, "require_edge");
                if(comparison != 0)
                {
                    comparison = strcmp(key, "min_edges");
                }
                if(comparison == 0)
                {
                    char   edge_name[EDGE_NAME_SIZE];
                    size_t required;
                    size_t actual;
                    size_t edge_count;

                    required         = 1U;
                    operation_status = snprintf(edge_name, sizeof(edge_name), "%s", value);
                    if(operation_status < 0 || (size_t)operation_status >= sizeof(edge_name))
                    {
                        operation_status = -1;
                        break;
                    }
                    operation_status = 0;
                    comparison       = strcmp(key, "min_edges");
                    if(comparison == 0)
                    {
                        char *count_separator;
                        char *end;

                        count_separator = strrchr(edge_name, ':');
                        if(count_separator == NULL)
                        {
                            operation_status = -1;
                            break;
                        }
                        *count_separator = '\0';
                        errno            = 0;
                        required         = strtoul(count_separator + 1, &end, DECIMAL_BASE);
                        if(errno != 0 || *end != '\0')
                        {
                            operation_status = -1;
                            break;
                        }
                    }
                    actual     = 0U;
                    edge_count = p101_tool_model_edge_count(loaded.model);
                    for(size_t index = 0U; index < edge_count; index++)
                    {
                        const struct p101_tool_model_edge *edge;
                        const char                        *name;

                        edge       = p101_tool_model_edge_at(loaded.model, index);
                        name       = model_edge_name(edge->kind);
                        comparison = strcmp(name, edge_name);
                        if(comparison == 0)
                        {
                            actual++;
                        }
                    }
                    satisfied = actual >= required;
                    goto expectation_checked;
                }
                comparison = strcmp(key, "require_call");
                if(comparison != 0)
                {
                    comparison = strcmp(key, "forbid_call");
                }
                if(comparison != 0)
                {
                    comparison = strcmp(key, "require_resource");
                }
                if(comparison == 0)
                {
                    bool   present;
                    size_t node_count;
                    bool   resource_expectation;

                    present              = false;
                    node_count           = p101_tool_model_node_count(loaded.model);
                    comparison           = strcmp(key, "require_resource");
                    resource_expectation = comparison == 0;
                    for(size_t index = 0U; index < node_count; index++)
                    {
                        const struct p101_tool_model_node *node;
                        const char                        *candidate_value;

                        node            = p101_tool_model_node_at(loaded.model, index);
                        candidate_value = NULL;
                        if(resource_expectation && node->domain == P101_TOOL_MODEL_NODE_RESOURCE)
                        {
                            candidate_value = node->resource_class;
                        }
                        else if(!resource_expectation && node->domain == P101_TOOL_MODEL_NODE_CALL && node->call_kind == P101_TOOL_EVENT_CALL_ENTER)
                        {
                            candidate_value = node->call_name;
                        }
                        if(candidate_value != NULL)
                        {
                            bool matches;

                            const char *expected_pattern;

                            expected_pattern = value;
                            matches          = pattern_matches(expected_pattern, candidate_value);
                            if(matches)
                            {
                                present = true;
                                break;
                            }
                        }
                    }
                    comparison = strcmp(key, "forbid_call");
                    if(comparison == 0)
                    {
                        satisfied = (_Bool)(!present);
                    }
                    else
                    {
                        satisfied = present;
                    }
                    goto expectation_checked;
                }
                operation_status = -1;
                break;
            }
        expectation_checked:
            if(!satisfied)
            {
                int diagnostic_status;

                diagnostic_status = fprintf(stderr, "%s:%zu:1: error: expectation not satisfied: %s=%s [P101-EXPECT-001]\n", expectations, line_number, key, value);
                (void)diagnostic_status;
                failures++;
            }
        next_expectation:
            line[0]     = '\0';
            read_result = fgets(line, sizeof(line), stream);
        }
        io_status = ferror(stream);
        if(io_status != 0)
        {
            operation_status = -1;
        }
        io_status = fclose(stream);
        if(io_status != 0)
        {
            operation_status = -1;
        }
        if(operation_status != 0)
        {
            unload_analysis(&loaded);
            p101_single_result_ = EXIT_TROUBLE;
            goto p101_single_exit_;
        }
    }
    {
        int    io_status;
        size_t node_count;
        size_t edge_count;
        size_t finding_count;

        node_count    = p101_tool_model_node_count(loaded.model);
        edge_count    = p101_tool_model_edge_count(loaded.model);
        finding_count = p101_tool_analysis_finding_count(loaded.analysis);
        io_status     = fprintf(stdout, "p101-inspect: verify: nodes=%zu edges=%zu findings=%zu result=%s\n", node_count, edge_count, finding_count, receipt_status == EXIT_SUCCESS ? "clean" : "findings");
        (void)io_status;
    }
    unload_analysis(&loaded);
    p101_single_result_ = failures == 0U ? EXIT_SUCCESS : EXIT_FINDINGS;

p101_single_exit_:
    return p101_single_result_;
}

// cppcheck-suppress constParameter ; keep the main-compatible argument-vector type.
static int model_check(const char *directory, int argc, char *const argv[])
{
    int                    p101_single_result_;
    struct loaded_analysis loaded;
    size_t                 violations;
    int                    operation_status;
    int                    comparison;

    operation_status = load_analysis_directory(directory, &loaded);
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    violations = 0U;
    for(int index = 0; index < argc; index += 2)
    {
        const char                        *pack_name;
        const struct rule_pack_definition *pack;

        comparison = -1;
        if(index + 1 < argc)
        {
            comparison = strcmp(argv[index], "--rules");
        }
        if(comparison != 0)
        {
            unload_analysis(&loaded);
            p101_single_result_ = EXIT_TROUBLE;
            goto p101_single_exit_;
        }
        pack_name = argv[index + 1];
        pack      = find_rule_pack(pack_name);
        if(pack == NULL)
        {
            unload_analysis(&loaded);
            p101_single_result_ = EXIT_TROUBLE;
            goto p101_single_exit_;
        }
        for(size_t rule_index = 0U; rule_index < pack->rule_count; rule_index++)
        {
            const struct rule_definition *rule;
            bool                          evidence_present;
            bool                          forbidden;
            bool                          violated;

            rule             = &pack->rules[rule_index];
            evidence_present = rule_has_evidence(&loaded, rule);
            comparison       = strncmp(rule->kind, "forbid-", sizeof("forbid-") - 1U);
            forbidden        = comparison == 0;
            if(forbidden)
            {
                violated = evidence_present;
            }
            else
            {
                violated = (_Bool)(!evidence_present);
            }
            if(violated)
            {
                int io_status;

                io_status = fprintf(stdout, "%s:1:1: error: %s [%s]\n  pattern: %s\n  lesson: %s\n", directory, rule->title, rule->identifier, rule->pattern, rule->lesson);
                (void)io_status;
                violations++;
            }
        }
    }
    unload_analysis(&loaded);
    p101_single_result_ = violations == 0U ? EXIT_SUCCESS : EXIT_FINDINGS;

p101_single_exit_:
    return p101_single_result_;
}

static int model_explain(const char *directory, const char *diagnostic_id)
{
    int                                      p101_single_result_;
    struct loaded_analysis                   loaded;
    const struct p101_tool_analysis_finding *selected;
    size_t                                   finding_count;
    int                                      operation_status;
    int                                      comparison;

    operation_status = load_analysis_directory(directory, &loaded);
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
        goto p101_single_exit_;
    }
    selected      = NULL;
    finding_count = p101_tool_analysis_finding_count(loaded.analysis);
    for(size_t index = 0U; index < finding_count; index++)
    {
        const struct p101_tool_analysis_finding *finding;

        finding    = p101_tool_analysis_finding_at(loaded.analysis, index);
        comparison = strcmp(finding->diagnostic_id, diagnostic_id);
        if(comparison == 0)
        {
            selected = finding;
            break;
        }
    }
    if(selected == NULL)
    {
        unload_analysis(&loaded);
        p101_single_result_ = EXIT_FINDINGS;
        goto p101_single_exit_;
    }
    {
        const struct p101_tool_rule_definition *lesson;
        int                                     io_status;

        io_status = fprintf(stdout, "# %s: %s\nsource=%s:%d function=%s\n", selected->diagnostic_id, selected->message, selected->file_name, selected->line_number, selected->function_name);
        (void)io_status;
        lesson = p101_tool_rule_definition_lookup_id(selected->diagnostic_id);
        if(lesson != NULL)
        {
            io_status = fprintf(stdout, "lesson=%s %s\n", lesson->lesson_id, lesson->lesson_url);
            (void)io_status;
        }
    }
    {
        size_t node_count;
        size_t edge_count;

        node_count = p101_tool_model_node_count(loaded.model);
        edge_count = p101_tool_model_edge_count(loaded.model);
        for(size_t edge_index = 0U; edge_index < edge_count; edge_index++)
        {
            const struct p101_tool_model_edge *edge;
            const struct p101_tool_model_node *from;
            const struct p101_tool_model_node *to;
            bool                               source_matches;
            int                                from_comparison;
            int                                to_comparison;

            edge            = p101_tool_model_edge_at(loaded.model, edge_index);
            from            = p101_tool_model_node_at(loaded.model, edge->from);
            to              = p101_tool_model_node_at(loaded.model, edge->to);
            from_comparison = -1;
            to_comparison   = -1;
            if(from->file_name != NULL)
            {
                from_comparison = strcmp(from->file_name, selected->file_name);
            }
            if(to->file_name != NULL)
            {
                to_comparison = strcmp(to->file_name, selected->file_name);
            }
            source_matches = (_Bool)(from_comparison == 0 || to_comparison == 0);
            if(source_matches)
            {
                int         io_status;
                const char *edge_name;

                edge_name = model_edge_name(edge->kind);
                io_status = fprintf(stdout, "edge %s %zu -> %zu\n", edge_name, edge->from, edge->to);
                (void)io_status;
            }
        }
        if(node_count == 0U)
        {
            int io_status;

            io_status = fputs("note: no run-model nodes were observed\n", stdout);
            (void)io_status;
        }
    }
    unload_analysis(&loaded);
    p101_single_result_ = EXIT_SUCCESS;

p101_single_exit_:
    return p101_single_result_;
}

static int model_compare(const char *before_directory, const char *after_directory)
{
    int  p101_single_result_;
    char before_path[PATH_SIZE];
    char after_path[PATH_SIZE];
    int  before_status;
    int  after_status;
    int  operation_status;
    bool different;

    before_status    = EXIT_TROUBLE;
    after_status     = EXIT_TROUBLE;
    different        = false;
    operation_status = analysis_result(before_directory, &before_status);
    if(operation_status == 0)
    {
        operation_status = analysis_result(after_directory, &after_status);
    }
    if(operation_status == 0)
    {
        operation_status = join_path(before_path, before_directory, "run-model.json");
    }
    if(operation_status == 0)
    {
        operation_status = join_path(after_path, after_directory, "run-model.json");
    }
    if(operation_status == 0)
    {
        operation_status = compare_file_pair(before_path, after_path, &different);
    }
    if(operation_status == 0 && !different)
    {
        operation_status = join_path(before_path, before_directory, "finding-index.tsv");
    }
    if(operation_status == 0 && !different)
    {
        operation_status = join_path(after_path, after_directory, "finding-index.tsv");
    }
    if(operation_status == 0 && !different)
    {
        operation_status = compare_file_pair(before_path, after_path, &different);
    }
    if(operation_status != 0)
    {
        p101_single_result_ = EXIT_TROUBLE;
    }
    else
    {
        if(different)
        {
            p101_single_result_ = EXIT_FINDINGS;
        }
        else
        {
            p101_single_result_ = EXIT_SUCCESS;
        }
    }
    return p101_single_result_;
}

static int compare_file_pair(const char *before_path, const char *after_path, bool *different)
{
    int   p101_single_result_;
    FILE *before_stream;
    FILE *after_stream;
    int   operation_status;
    int   left;
    int   close_status;

    before_stream    = fopen(before_path, "re");
    after_stream     = fopen(after_path, "re");
    operation_status = 0;
    *different       = false;
    if(before_stream == NULL || after_stream == NULL)
    {
        operation_status = -1;
    }
    left = 0;
    while(operation_status == 0 && left != EOF)
    {
        int right;

        left  = fgetc(before_stream);
        right = fgetc(after_stream);
        if(left != right)
        {
            *different = true;
            break;
        }
    }
    if(before_stream != NULL)
    {
        close_status = fclose(before_stream);
        if(close_status != 0)
        {
            operation_status = -1;
        }
    }
    if(after_stream != NULL)
    {
        close_status = fclose(after_stream);
        if(close_status != 0)
        {
            operation_status = -1;
        }
    }
    p101_single_result_ = operation_status;
    return p101_single_result_;
}
