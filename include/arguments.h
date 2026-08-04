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
    char *const *command_argv;
    bool         verbose;
    bool         log_arguments;
    bool         log_results;
    bool         show_help;
};

#endif    // P101_OBSERVE_ARGUMENTS_H
