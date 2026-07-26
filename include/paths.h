#ifndef P101_OBSERVE_PATHS_H
#define P101_OBSERVE_PATHS_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

struct report_paths
{
    char dir[PATH_LEN];
    char command[PATH_LEN];
    char stdout_text[PATH_LEN];
    char stderr_text[PATH_LEN];
    char resource_log[PATH_LEN];
    char call_log[PATH_LEN];
    char resource_report[PATH_LEN];
    char resource_json[PATH_LEN];
    char resource_stderr[PATH_LEN];
    char trace_tree[PATH_LEN];
    char trace_summary[PATH_LEN];
    char trace_stderr[PATH_LEN];
    char correlated_report[PATH_LEN];
    char correlated_json[PATH_LEN];
    char report_stderr[PATH_LEN];
    char summary[PATH_LEN];
};

void p101_observe_make_report_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct report_paths *paths);
void p101_observe_join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
void p101_observe_create_report_dir(const struct p101_env *env, struct p101_error *err, const char *path);
void p101_observe_create_empty_file(const struct p101_env *env, struct p101_error *err, const char *path);
void p101_observe_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *command_argv);

#endif    // P101_OBSERVE_PATHS_H
