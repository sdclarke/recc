# recc
Compiler wrapper client for the Remote Execution API.

## Compiling

`recc` depends on [gRPC][], [Protobuf][], [OpenSSL][], and [CMake][], so you'll
need to install those first using your operating system's package manager.

[grpc]: https://grpc.io/
[protobuf]: https://github.com/google/protobuf/
[openssl]: https://www.openssl.org/
[cmake]: https://cmake.org/

Once you've done that, you can compile `recc` using the following commands:

```sh
$ mkdir build
$ cd build
$ cmake .. && make
```

### Debugging options

You can define `RECC_DEBUG` while running `cmake` to include additional debugging info in the final binaries.
Just include `-DRECC_DEBUG` when invoking `cmake`.

Compiling with this flag will include the function name and line number every time `RECC_LOG_VERBOSE` is called.
(Note that you still need to run in verbose mode to receive the output)

## Running tests

To run tests, first compile the project (see above), then run
`make test`.

## Running `recc`

`recc` is a command-line utility that runs compile commands on a Remote
Execution server. To run it, you'll first need to set up a server for it to
talk to. Then, set the appropriate environment variables and call `recc`
with a compile command.

### Setting up a Remote Execution server

`recc` should be compatible with any server that implements Bazel's
[Remote Execution API][] V2. It is primarily tested against [BuildGrid][],
so follow the instructions there to start a server.

You'll also need to set up a build worker to actually run the build jobs
you submit. BuildGrid provides one you can use, or alternatively, you can
use [`reccworker` (see below)](#running-reccworker).

The [Bazel Buildfarm][] project is also working on a Remote Execution server,
but at time of writing it uses version 1 of the API and so cannot be used with
`recc`.

[remote execution api]: https://github.com/bazelbuild/remote-apis
[buildgrid]: https://gitlab.com/BuildGrid/buildgrid
[bazel buildfarm]: https://github.com/bazelbuild/bazel-buildfarm

### Setting environment variables

`recc` uses environment variables to configure its behavior. To see a full list
of the environment variables `recc` accepts and what they do, run
`recc --help`.

At minimum, you'll need to set `RECC_SERVER` to the URI of your Remote
Execution server:

```sh
$ export RECC_SERVER=localhost:12345
```

You may also want to set `RECC_VERBOSE=1` to enable verbose output.

#### Additional variables needed on AIX

On AIX, `recc` injects a dynamic library called [`libreccdevshim`][] to
work around issues in `xlc`. You'll need to set two environment variables to
tell it where to find the library:

```sh
$ export RECC_DEPS_ENV_LDR_PRELOAD=/path/to/libreccdevshim.so
$ export RECC_DEPS_ENV_LDR_PRELOAD64=/path/to/64bit/libreccdevshim.so
```

[`libreccdevshim`]: src/lib/reccdevshim

### Calling `recc` with a compile command

Once you've started the server and set the environment variables, you're ready
to call `recc` with a compile command: 

```sh
$ recc gcc -c hello.c -o hello.o
```

`recc` only supports compilation, not linking, so be sure to include the `-c`
argument in your command. If `recc` doesn't think your command is a compile
command, it'll just run it locally:

```sh
$ recc echo hello
hello
$ RECC_VERBOSE=1 recc echo hello
Not a compiler command, so running locally.
(use RECC_FORCE_REMOTE=1 to force remote execution)
hello
$
```

## Using `reccworker`

`reccworker` is a prototype Remote Workers API client. It polls a server for
pending jobs, runs them, and sends the results back.

Note that `reccworker` does not attempt to sandbox builds (they're run with
the full privileges of the user who ran `reccworker`) and does not enforce
timeouts.

Bazel Buildfarm does not support the Remote Workers API, so you'll need to
use BuildGrid to run `reccworker`. Just like with `recc`, you can tell
`reccworker` where your server is using the `RECC_SERVER` environment variable.

You will probably also want to set `RECC_MAX_CONCURRENT_JOBS` to the number of
simultaneous build jobs you want the worker to run. (By default, it runs jobs
one at a time.) Run `reccworker --help` for the full list of environment
variables `reccworker` accepts.

Once you've set up the servers and environment, start the worker by running
`reccworker`. You can optionally specify a parent (the default is `bgd_test`)
and bot name (the default is the computer's hostname) by passing them as
arguments.

## Additional utilities

This repo includes a couple of additional utilities that may be useful for
debugging. They aren't installed by default, but running `make` will place
them in the `bin` subdirectory of the project root.

- `deps [command]` - Print the names of the files needed to run the given
  command. (`recc` uses this to decide which files to send to the Remote
  Execution server.)

- `casupload [files]` - Upload the given files to CAS, then print the digest
  hash and size of the resulting Directory message.
