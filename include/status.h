#ifndef P101_OBSERVE_STATUS_H
#define P101_OBSERVE_STATUS_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdio.h>

bool p101_observe_status_is_success(int status);
bool p101_observe_tool_status_is_acceptable(int status);
void p101_observe_print_status(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);

#endif    // P101_OBSERVE_STATUS_H
