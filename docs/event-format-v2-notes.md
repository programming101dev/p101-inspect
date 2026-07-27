# p101 event format v2 notes

Version 1 is intentionally small and readable. It records sequence by line order,
but it does not record time. That choice keeps the format approachable, but it
forces tools to derive ordering when several processes write to the same stream.

Version 2 adds timing without making the format feel like a black box.

Status: implemented as an opt-in format. Version 1 remains the default;
`P101_EVENT_LOG_VERSION=2` or `p101_env_set_event_log_version()` enables v2
emission, and the core consumers read both versions.

## Proposed additions

Every v2 event includes:

- `seq`: a per-process monotonically increasing event number;
- `mono_ns`: a monotonic timestamp in nanoseconds when available;
- `wall_unix_ns`: a wall-clock timestamp in Unix nanoseconds when available.

For plain text, the timestamp fields should appear near the front so generic
tools can sort or group records without understanding every event kind:

```text
P101FD<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>OPEN|CLOSE...
P101ALLOC<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>ALLOC|FREE|REALLOC...
P101CALL<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>ENTER|EXIT...
```

If a platform cannot provide a timestamp, use `-` for that field. Do not silently
substitute wall-clock time for monotonic time.

## Compatibility rule

Version 1 remains the default so old assignments and expected outputs stay
stable. Readers that opt into v2 support accept versions 1 and 2 and still
reject newer versions explicitly.

## Why it matters

Timestamps unlock:

- fork/interleaving diagnostics that are not purely line-order dependent;
- descriptor and allocation lifetime duration;
- slow-wrapper reports;
- “where did the program block?” discussions for systems classes.
