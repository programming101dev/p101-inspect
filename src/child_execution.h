#ifndef P101_OBSERVE_CHILD_EXECUTION_H
#define P101_OBSERVE_CHILD_EXECUTION_H

#include "arguments.h"
#include "paths.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

int p101_observe_run_observed_command(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths);
int p101_observe_run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path);

#endif
