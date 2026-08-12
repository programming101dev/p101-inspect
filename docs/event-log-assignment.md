# Assignment: write a p101 event-log analyzer

The p101 tools intentionally use plain tab-separated text logs. That makes the
format inspectable with `cat`, `awk`, `sed`, Python, C, or any other tool the
course wants students to practice.

For this assignment, students build a tiny analyzer over `resources.log`.

## Input

Use a `resources.log` produced by `inspect-capture`. The format is documented in
the
[`lib_tool_event` protocol specification](https://github.com/programming101dev/lib_tool_event/blob/main/docs/event-format.md).

The analyzer must ignore lines that do not begin with a p101 record prefix.

## Minimum requirements

Track descriptor state:

1. On `P101FD ... OPEN ...`, mark `(pid, fd)` as live.
2. On `P101FD ... CLOSE ...`, remove `(pid, fd)` if it is live.
3. If a close arrives for a descriptor that is not live, report it.
4. At end of file, report all descriptors still live.

Then do the same state-machine exercise for allocations:

1. On `P101ALLOC ... ALLOC ...`, mark `(pid, ptr)` as live.
2. On `P101ALLOC ... FREE ...`, remove `(pid, ptr)` if it is live.
3. On `P101ALLOC ... REALLOC ...`, move the old pointer to the new pointer.
4. At end of file, report all pointers still live.

## Stretch goals

- Count findings by stable diagnostic ID.
- Emit JSON.
- Attribute each finding to the source file/function/line fields in the log.
- Keep a peak heap watermark by summing live allocation sizes.
- Write the descriptor half in `awk` and the full analyzer in C.

## Discussion questions

- Why does the analyzer need `(pid, fd)` instead of just `fd`?
- What does the analyzer miss if a program calls `malloc` instead of
  `p101_malloc`?
- Why is a double close sometimes security-relevant?
- What extra fields would make forked-process ordering easier to reason about?

## Instructor note

This assignment teaches the same state-machine reasoning used by
the canonical `p101-inspect analyze` resource policy, without requiring students to understand the full
implementation first. The point is not to write the fastest analyzer; the point
is to make ownership visible.
