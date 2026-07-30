#ifndef P101_OBSERVE_ARGUMENTS_H
#define P101_OBSERVE_ARGUMENTS_H

#include <stdbool.h>

enum
{
    PATH_LEN = 1024
};

struct arguments
{
    const char  *report_dir;
    const char  *resource_tracker;
    const char  *p101_sync_check;
    const char  *p101_trace;
    const char  *p101_report;
    char *const *command_argv;
    bool         verbose;
    bool         log_arguments;
    bool         log_results;
    bool         capture_only;
};

#endif    // P101_OBSERVE_ARGUMENTS_H
