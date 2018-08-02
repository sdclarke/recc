# libreccdevshim
Library to emulate `/dev/stdin`, `/dev/stdout`, and `/dev/stderr` on AIX

When loaded with `LD_PRELOAD`, `LDR_PRELOAD`, or equivalent, `libreccdevshim`
overrides the `open`, `creat`, and `fopen` system calls to allow programs
to open their standard input, output, and error streams as though they were
files. This functionality exists on Linux and Solaris by default, but it's
not present on AIX.

## Motivation

`recc` uses `libreccdevshim` to work around an issue in the `xlc` compiler.
Specifically, when `xlc` tries to write dependency information for multiple
source files to a single dependency file, it repeatedly overwrites the
dependency file instead of appending to it. (In other words, only the last
source file's dependencies actually get written.)

If we convince `xlc` to write dependency information to stdout instead of a
file (by injecting `libreccdevshim` and passing `/dev/stdout` as the output
path), we can prevent it from deleting the information.

## Compiling

`libreccdevshim` is a single C source file with no dependencies, so you can
compile it on AIX with the following commands:

```sh
$ xlc -G -o libreccdevshim.so.32 reccdevshim.c -q32
$ xlc -G -o libreccdevshim.so.64 reccdevshim.c -q64
```

A CMakeLists file is also included if you'd prefer to build with CMake.
