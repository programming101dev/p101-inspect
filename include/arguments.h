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
    const char  *p101_trace;
    char *const *command_argv;
    bool         verbose;
};

#endif    // P101_OBSERVE_ARGUMENTS_H
