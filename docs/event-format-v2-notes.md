# p101 event format v2 notes

Version 2 is the supported runtime event schema. It records enough timing
metadata for reports to talk about resource lifetimes without turning the log
into an opaque binary trace.

Every event includes:

- `seq`: a per-env monotonically increasing event number;
- `mono_ns`: a monotonic timestamp in nanoseconds when available;
- `wall_unix_ns`: a wall-clock timestamp in Unix nanoseconds when available.

The fields appear near the front so generic tools can sort or group records
without understanding every event kind:

```text
P101FD<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>OPEN|CLOSE...
P101ALLOC<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>ALLOC|FREE|REALLOC...
P101CALL<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>ENTER|EXIT...
```

If a platform cannot provide a timestamp, `lib_env` writes `-` for that field.
It does not silently substitute wall-clock time for monotonic time.

Timestamps unlock:

- descriptor and allocation lifetime duration;
- better fork/interleaving diagnostics than plain line-order;
- future slow-wrapper and blocking-I/O reports.
