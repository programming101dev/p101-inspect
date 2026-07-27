# p101 event format v1/v2

The p101 runtime tools read plain-text, tab-separated records emitted by
`lib_env`. The format is intentionally small enough for students to inspect by
hand and strict enough for tools to replay safely.

All records are one physical line. Fields that may contain tabs, newlines,
carriage returns, or backslashes are escaped by `lib_env`. Consumers should use
`p101_env_read_event_line()` for byte-safe physical-line input, ignore lines
whose first field is not a p101 record prefix, count malformed p101 records, and
reject unsupported record versions.

`p101_env_read_event_line()` deliberately does less than a schema parser: it
only distinguishes OK, EOF, malformed text, and I/O error. It marks embedded NUL
bytes and overlong physical lines as malformed so a damaged log cannot be
silently replayed as a different record.

Version 1 is the stable default. Version 2 is opt-in with
`P101_EVENT_LOG_VERSION=2` or `p101_env_set_event_log_version()` and inserts
three metadata fields after pid:

```text
MAGIC<TAB>2<TAB>pid<TAB>seq<TAB>mono_ns<TAB>wall_unix_ns<TAB>...
```

Tools may add derived event numbers in their reports. A derived event number is
the 1-based sequence of successfully parsed records in that input stream; it is
stable for a specific log file and is used to correlate trace context, resource
lifetimes, and findings. In v2 logs, `seq` is emitted by `lib_env` as a per-env
monotonic event number, `mono_ns` is monotonic nanoseconds when available, and
`wall_unix_ns` is Unix wall-clock nanoseconds when available. Unavailable
timestamps are written as `-`.

## Resource records

Descriptor events:

```text
P101FD<TAB>1<TAB>pid<TAB>OPEN|CLOSE<TAB>fd<TAB>line<TAB>function<TAB>file
```

Allocation events:

```text
P101ALLOC<TAB>1<TAB>pid<TAB>ALLOC|FREE|REALLOC<TAB>ptr<TAB>new_ptr<TAB>size<TAB>line<TAB>function<TAB>file
```

Fork events:

```text
P101FORK<TAB>1<TAB>parent-pid<TAB>child-pid<TAB>line<TAB>function<TAB>file
```

Exec-boundary descriptor snapshots:

```text
P101EXEC<TAB>1<TAB>pid<TAB>fd<TAB>cloexec<TAB>line<TAB>function<TAB>file<TAB>target
```

`new_ptr` is `-` when there is no second pointer. Pointer strings are opaque
because `%p` is platform formatted. `cloexec` is `0` when `FD_CLOEXEC` is off
and `1` when it is on. `target` is the path/file argument passed to the exec
wrapper. Fields that may contain control characters are escaped by `lib_env`.

## Call records

```text
P101CALL<TAB>1<TAB>pid<TAB>ENTER|EXIT<TAB>line<TAB>function<TAB>call<TAB>args<TAB>result<TAB>file
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

Version 1 records do not contain wall-clock timestamps or monotonic timestamps.
Tools therefore report ordering and resource age for v1 logs by derived event
number rather than elapsed time. Version 2 records provide timing metadata, but
the current reports still keep derived event IDs for stable, human-readable
correlation across v1 and v2 inputs.
