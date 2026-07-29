# p101 event format v2

The p101 runtime tools read plain-text, tab-separated records emitted by
`lib_env`. The format is intentionally small enough for students to inspect by
hand and strict enough for tools to replay safely.

All records are one physical line. Fields that may contain tabs, newlines,
carriage returns, or backslashes are escaped by `lib_env`. Consumers should use
`p101_env_read_event_line()` for byte-safe physical-line input and
`p101_env_parse_event_line()` for the shared schema parser.

`p101_env_read_event_line()` deliberately does less than a schema parser: it
only distinguishes OK, EOF, malformed text, and I/O error. It marks embedded NUL
bytes and overlong physical lines as malformed so a damaged log cannot be
silently replayed as a different record.

Every supported event uses version `2` and includes sequence and timestamp
metadata immediately after pid:

```text
MAGIC<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>...
```

`seq` is emitted by `lib_env` as a per-env monotonically increasing event
number. `mono_ns` is monotonic nanoseconds when available, and `wall_unix_ns` is
Unix wall-clock nanoseconds when available. Unavailable timestamps are written
as `-`.

Tools may still add derived event numbers in their reports. A derived event
number is the 1-based sequence of successfully parsed records in that input
stream; it is stable for a specific log file and useful when a tool reads only
one stream. Reports that show resource lifetime duration use the v2 monotonic
timestamps when both endpoints have them.

## Resource records

Descriptor events:

```text
P101FD<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>OPEN|CLOSE<TAB>fd<TAB>line<TAB>function<TAB>file
```

Allocation events:

```text
P101ALLOC<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>ALLOC|FREE|REALLOC<TAB>ptr<TAB>new_ptr<TAB>size<TAB>line<TAB>function<TAB>file
```

Fork events:

```text
P101FORK<TAB>2<TAB>parent-pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>child-pid<TAB>line<TAB>function<TAB>file
```

Spawn events:

```text
P101SPAWN<TAB>2<TAB>parent-pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>child-pid<TAB>line<TAB>function<TAB>file<TAB>target
```

POSIX spawn file actions are opaque. Consumers retain this boundary but must
not infer a fork-equivalent child descriptor table from it.

Exec-boundary descriptor snapshots:

```text
P101EXEC<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>fd<TAB>cloexec<TAB>line<TAB>function<TAB>file<TAB>target
```

Failed exec attempts:

```text
P101EXECFAIL<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>line<TAB>function<TAB>file<TAB>target
```

`new_ptr` is `-` when there is no second pointer. Pointer strings are opaque
because `%p` is platform formatted. `cloexec` is `0` when `FD_CLOEXEC` is off
and `1` when it is on. `target` is the path/file argument passed to the exec
wrapper.
An exec wrapper writes its descriptor snapshots before calling the native
function. If that call returns, `P101EXECFAIL` tells consumers to roll back the
inheritance findings for that attempt; no new program image was entered.

## Call records

```text
P101CALL<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>ENTER|EXIT<TAB>line<TAB>function<TAB>call<TAB>args<TAB>result<TAB>file
```

`args` and `result` are `-` when they were not logged or do not apply. Argument
and return-value logging is optional and controlled by the environment bridge or
`p101_env_set_call_log()` options.

## Fault records

Fault-injection records are a control stream for `p101-error-path-walk`:

```text
P101FAULT<TAB>1<TAB>pid<TAB>call-index<TAB>call-name<TAB>errno
```

They are intentionally separate from the resource and call logs.

## Current limitations

The tools only see wrapper events routed through `lib_env`. Direct libc calls,
third-party code, and kernel-internal resource activity are outside the log.
When several processes write to the same stream, v2 timestamps make reports more
precise, but the log is still a teaching trace, not a kernel-level audit trail.
