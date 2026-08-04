#ifndef P101_OBSERVE_PATHS_H
#define P101_OBSERVE_PATHS_H

#include "arguments.h"
#include "constants.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

struct report_paths
{
    char run_id[RUN_ID_LEN];
    char dir[PATH_LEN];
    char command[PATH_LEN];
    char stdout_text[PATH_LEN];
    char stderr_text[PATH_LEN];
    char resource_log[PATH_LEN];
    char call_log[PATH_LEN];
    char summary[PATH_LEN];
    char manifest[PATH_LEN];
    char receipt[PATH_LEN];
    char tool_receipt[PATH_LEN];
};

void p101_observe_make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths);
void p101_observe_join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
void p101_observe_create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path);
void p101_observe_create_empty_file(const struct p101_env *env, struct p101_error *err, const char *path);
void p101_observe_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *command_argv);
void p101_observe_write_manifest_file(const struct p101_env *env, struct p101_error *err, const char *path, const struct arguments *args, const struct report_paths *paths);

#endif    // P101_OBSERVE_PATHS_H
