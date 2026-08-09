set(PROJECT_NAME "p101-inspect")
set(PROJECT_VERSION "2.0.0")
set(PROJECT_DESCRIPTION "Captures program events for interactive inspection")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Common compiler flags
set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)

set(DARWIN_STANDARD_FLAGS
        -D_DARWIN_C_SOURCE
)

set(LINUX_STANDARD_FLAGS
)

set(BSD_STANDARD_FLAGS
)

# Define targets
set(EXECUTABLE_TARGETS inspect_capture)
set(LIBRARY_TARGETS "")
set(inspect_capture_OUTPUT_NAME inspect-capture)

set(inspect_capture_SOURCES
    src/cli.c
    src/child_execution.c
    src/main.c
        src/paths.c
        src/report.c
        src/runner.c
        src/status.c
)

set(inspect_capture_HEADERS
        include/arguments.h
        include/cli.h
        include/constants.h
        include/errors.h
        include/paths.h
        include/report.h
        include/runner.h
        include/status.h
)

set(inspect_capture_LINK_LIBRARIES
        p101_error
        p101_env
        p101_tool_event
        p101_c
        p101_cli
        p101_filesystem
        p101_host
        p101_io
        p101_process
        p101_time
        p101_util
        m
)
