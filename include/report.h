#ifndef P101_OBSERVE_REPORT_H
#define P101_OBSERVE_REPORT_H

#include "arguments.h"
#include "paths.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

struct observe_result
{
    int command_status;
};

void p101_observe_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_paths *paths, const struct observe_result *result);
void p101_observe_write_receipt_file(const struct p101_env *env, struct p101_error *err, const struct report_paths *paths, const struct observe_result *result);

#endif    // P101_OBSERVE_REPORT_H
