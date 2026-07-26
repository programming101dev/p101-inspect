#ifndef P101_OBSERVE_RESOURCE_H
#define P101_OBSERVE_RESOURCE_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

struct resource_summary
{
    size_t records;
    size_t fd_leaks;
    size_t allocation_leaks;
    size_t bad_releases;
    bool   parsed;
};

void   p101_observe_read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary);
bool   p101_observe_parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary);
bool   p101_observe_parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value);
size_t p101_observe_resource_finding_count(const struct resource_summary *summary);

#endif    // P101_OBSERVE_RESOURCE_H
