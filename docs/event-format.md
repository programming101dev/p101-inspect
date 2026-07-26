# p101 event format v1

The p101 runtime tools read plain-text, tab-separated records emitted by
`lib_env`. The format is intentionally small enough for students to inspect by
hand and strict enough for tools to replay safely.

All records are one physical line. Fields that may contain tabs, newlines,
carriage returns, or backslashes are escaped by `lib_env`. Consumers should
ignore lines whose first field is not a p101 record prefix, count malformed p101
records, and reject unsupported record versions.

The raw record version is currently `1`. Tools may add derived event numbers in
their reports. A derived event number is the 1-based sequence of successfully
parsed records in that input stream; it is stable for a specific log file and is
used to correlate trace context, resource lifetimes, and findings.

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

`new_ptr` is `-` when there is no second pointer. Pointer strings are opaque
because `%p` is platform formatted. `file` is intentionally last.

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
Tools therefore report ordering and resource age by derived event number rather
than elapsed time. If/when timing is added, it should be a versioned extension
that leaves v1 consumers able to reject or explicitly opt into the new shape.
