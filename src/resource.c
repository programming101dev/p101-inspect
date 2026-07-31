#include "resource.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdlib.h>

void p101_observe_read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary)
{
    FILE  *stream;
    char   buffer[JSON_BUF_LEN];
    size_t used;

    P101_TRACE_SCOPE(env);
    p101_memset(env, summary, 0, sizeof(*summary));
    stream = p101_fopen(env, err, path, "r");
    used   = 0;

    if(stream == NULL)
    {
        goto done;
    }

    while(used < sizeof(buffer) - 1U)
    {
        const char *line;

        if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- prior wrapper failure propagation
        {
            break;    // GCOVR_EXCL_LINE
        }
        line = p101_fgets(env, err, buffer + used, (int)(sizeof(buffer) - used), stream);

        if(line == NULL)
        {
            break;
        }

        used = p101_strlen(env, buffer);
    }

    buffer[used] = '\0';
    (void)p101_observe_parse_resource_summary(env, buffer, summary);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

bool p101_observe_parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary)
{
    (void)env;
    return p101_tool_event_parse_resource_summary_json(text, summary);
}

size_t p101_observe_resource_finding_count(const struct resource_summary *summary)
{
    size_t count;

    count = 0;

    if(summary->parsed)
    {
        count = p101_tool_event_resource_summary_finding_count(summary);
    }

    return count;
}
