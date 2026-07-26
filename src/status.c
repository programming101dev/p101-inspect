#include "status.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

bool p101_observe_status_is_success(int status)
{
    bool success;

    success = false;

    if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
    {
        success = true;
    }

    return success;
}

bool p101_observe_tool_status_is_acceptable(int status)
{
    bool acceptable;

    acceptable = false;

    if(WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS || WEXITSTATUS(status) == EXIT_FINDINGS))
    {
        acceptable = true;
    }

    return acceptable;
}

void p101_observe_print_status(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        p101_fprintf(env, err, stream, "%s: exit=%d\n", label, WEXITSTATUS(status));
    }
    else if(WIFSIGNALED(status))
    {
        p101_fprintf(env, err, stream, "%s: signal=%d\n", label, WTERMSIG(status));
    }
    else
    {
        p101_fprintf(env, err, stream, "%s: status=%d\n", label, status);
    }
}
