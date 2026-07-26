#ifndef P101_OBSERVE_CLI_H
#define P101_OBSERVE_CLI_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_observe_arguments_init(const struct p101_env *env, struct arguments *args);
void p101_observe_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
void p101_observe_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
_Noreturn void p101_observe_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

#endif    // P101_OBSERVE_CLI_H
