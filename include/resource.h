#ifndef P101_OBSERVE_RESOURCE_H
#define P101_OBSERVE_RESOURCE_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_tool_event/summary.h>

#define resource_summary p101_tool_event_resource_summary

void   p101_observe_read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary);
bool   p101_observe_parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary);
size_t p101_observe_resource_finding_count(const struct resource_summary *summary);

#endif    // P101_OBSERVE_RESOURCE_H
