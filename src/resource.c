#include "resource.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <stdint.h>
#include <stdlib.h>

void p101_observe_read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary)
{
    FILE  *stream;
    char   buffer[JSON_BUF_LEN];
    size_t used;

    P101_TRACE(env);
    p101_memset(env, summary, 0, sizeof(*summary));
    stream = p101_fopen(env, err, path, "r");
    used   = 0;

    if(stream == NULL)
    {
        goto done;
    }

    while(p101_error_has_no_error(err) && used < sizeof(buffer) - 1U)
    {
        const char *line;

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
    bool parsed;
    bool records_parsed;
    bool fd_leaks_parsed;
    bool allocation_leaks_parsed;
    bool bad_releases_parsed;
    bool exec_inheritances_parsed;
    bool malformed_parsed;
    bool bad_version_parsed;
    bool refused_parsed;

    records_parsed           = p101_observe_parse_json_size(env, text, JSON_RECORDS, &summary->records);
    fd_leaks_parsed          = p101_observe_parse_json_size(env, text, JSON_FD_LEAKS, &summary->fd_leaks);
    allocation_leaks_parsed  = p101_observe_parse_json_size(env, text, JSON_ALLOCATION_LEAKS, &summary->allocation_leaks);
    bad_releases_parsed      = p101_observe_parse_json_size(env, text, JSON_BAD_RELEASES, &summary->bad_releases);
    exec_inheritances_parsed = p101_observe_parse_json_size(env, text, JSON_EXEC_INHERITANCES, &summary->exec_inheritances);
    malformed_parsed         = p101_observe_parse_json_size(env, text, JSON_MALFORMED, &summary->malformed);
    bad_version_parsed       = p101_observe_parse_json_size(env, text, JSON_BAD_VERSION, &summary->bad_version);
    refused_parsed           = p101_observe_parse_json_size(env, text, JSON_REFUSED, &summary->refused);
    if(!exec_inheritances_parsed)
    {
        summary->exec_inheritances = 0U;
    }
    if(!malformed_parsed)
    {
        summary->malformed = 0U;
    }
    if(!bad_version_parsed)
    {
        summary->bad_version = 0U;
    }
    if(!refused_parsed)
    {
        summary->refused = 0U;
    }
    parsed          = (records_parsed && fd_leaks_parsed && allocation_leaks_parsed && bad_releases_parsed) != 0;
    summary->parsed = parsed;

    return parsed;
}

bool p101_observe_parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value)
{
    const char *cursor;
    size_t      parsed;
    bool        ok;

    ok     = false;
    cursor = p101_strstr(env, text, key);

    if(cursor == NULL)
    {
        goto done;
    }

    cursor = p101_strchr(env, cursor, ':');

    if(cursor == NULL)
    {
        goto done;
    }

    cursor++;
    while(*cursor == ' ' || *cursor == '\t')
    {
        cursor++;
    }

    if(*cursor < '0' || *cursor > '9')
    {
        goto done;
    }

    parsed = 0U;
    while(*cursor >= '0' && *cursor <= '9')
    {
        size_t digit;

        digit = (size_t)(*cursor - '0');
        if(parsed > (SIZE_MAX - digit) / (size_t)JSON_NUMBER_BASE)
        {
            goto done;
        }
        parsed = (parsed * (size_t)JSON_NUMBER_BASE) + digit;
        cursor++;
    }

    *value = parsed;
    ok     = true;

done:
    return ok;
}

size_t p101_observe_resource_finding_count(const struct resource_summary *summary)
{
    size_t count;

    count = 0;

    if(summary->parsed)
    {
        count = summary->fd_leaks + summary->allocation_leaks + summary->bad_releases + summary->exec_inheritances + summary->malformed + summary->bad_version + summary->refused;
    }

    return count;
}
