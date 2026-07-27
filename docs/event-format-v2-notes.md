# p101 event format v2 notes

Version 1 is intentionally small and readable. It records sequence by line order,
but it does not record time. That choice keeps the format approachable, but it
forces tools to derive ordering when several processes write to the same stream.

Version 2 should add timing without making the format feel like a black box.

Status: not implemented. The current implementation is v1 plus a shared
byte-safe line reader in `lib_env`. That reader was extracted first so v1 tools
handle damaged logs consistently while v2 remains a deliberate format change.

## Proposed additions

Every v2 event should include:

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

Version 1 readers should continue rejecting unsupported versions. Version 2
support should be explicit in each reader so old assignments and old expected
outputs remain stable.

## Why it matters

Timestamps unlock:

- fork/interleaving diagnostics that are not purely line-order dependent;
- descriptor and allocation lifetime duration;
- slow-wrapper reports;
- “where did the program block?” discussions for systems classes.
