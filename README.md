# recc
Compiler wrapper client for the Remote Execution API.

For information regarding contributing to this project, please read the [Contribution guide](CONTRIBUTING.md).

# Installation

## Dependencies
Currently recc relies on: 

* [gRPC][]
* [Protobuf][] 
* [OpenSSL][]
* [CMake][]
* [GoogleTest][]
* [pkg-config][]

Some package managers (apt) include all these dependencies while others (brew) require manual installation of some. Please follow the relevant guides for your OS.  

### Installing on macOS

Install [gRPC][], [Protobuf][], [OpenSSL][], [CMake][], [pkg-config][] through brew:

```sh
$ brew install grpc protobuf openssl cmake pkg-config
```

[GoogleTest][] is not available in Homebrew, so instead, you should download a copy of [its source code][googletest source], unzip it somewhere, and use `-DGTEST_SOURCE_ROOT` to tell CMake where to find it. (See below.)

You can now compile `recc`.

<!-- # Linux -->
<!-- UPDATE ME -->

<!-- Reference links -->
[grpc]: https://grpc.io/
[protobuf]: https://github.com/google/protobuf/
[openssl]: https://www.openssl.org/
[cmake]: https://cmake.org/
[googletest]: https://github.com/google/googletest
[googletest source]: https://github.com/google/googletest/archive/release-1.8.1.zip
[pkg-config]: https://www.freedesktop.org/wiki/Software/pkg-config/

## Compiling
Once you've [installed the dependencies](#dependencies), you can compile `recc` using the following commands:

```sh
$ mkdir build
$ cd build
$ cmake .. && make
```

Note that on macOS, you'll need to manually specify the locations of OpenSSL and GoogleTest when running CMake:

```sh
$ cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DGTEST_SOURCE_ROOT=/wherever/you/unzipped/googletest/to .. && make
```

### Debugging options

You can define `RECC_DEBUG` while running `cmake` to include additional debugging info in the final binaries.
Just include `-DRECC_DEBUG` when invoking `cmake`.

Compiling with this flag will include the _function name_ and _line number_ every time `RECC_LOG_VERBOSE` is called.
(Note that you still need to run in verbose mode to receive the output)

### Running tests

To run tests, first compile the project (see above), then run:
```sh
$ make test
```

## Running `recc`

`recc` is a command-line utility that runs compile commands on a Remote
Execution server. To run it, you'll first need to set up an execution server 
for it to talk to. Then, set the appropriate configuration options
and call `recc` with a compile command.

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

### Configuration

`recc`’s default behavior can be overridden by configuration file settings, which in turn can be overridden by environment variables with names starting with RECC_. `recc` normally reads configuration from files in 3 locations. The priority of configuration settings is as follows (where 1 is highest):

  1) Environment variables
   
  2) User defined locations specified in DEFAULT_RECC_CONFIG_LOCATIONS, in file recc_defaults.h.

  3) ${cwd}/recc/recc.conf

  4) ~/.recc/recc.conf
   
  5) ${INSTALL_DIR}/../etc/recc/recc.conf
   
In addition, you can specify a config file location by adding it to: DEFAULT_RECC_CONFIG_LOCATIONS, in recc_defaults.h

#### Configuration File Syntax
Configuration files are in a simple “key = value” format, one setting per line. Lines starting with a hash sign are comments. Blank lines are ignored, as is whitespace surrounding keys and values.

At minimum, you'll need to set `RECC_SERVER` to the URI of your Remote:
```
  # set execution server to localhost:12345
  server = localhost:12345
```

For a full list of the environment/configuration variables `recc` accepts and what they do, run
`recc --help`.

If variables are specified in the configuration file, the prefix **RECC_** should not be included. 

#### Environment Variables

If you'd like to set environment variables, an example is specified below:

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

### Running `recc` against Google's RBE (Remote Build Execution) API

*NOTE:* At time of writing, RBE is still in alpha and instructions are subject
to change

To run `recc` against Google's RBE instead of a self hosted Remote Execution
Server, the following options need to be set:
 * `RECC_SERVER_AUTH_GOOGLEAPI=1` to enable using Google's default authentication
 * `RECC_INSTANCE=projects/<project_ID>/instances/default_instance` Where <project_ID> is the id of your Google Cloud Platform project
 * `RECC_SERVER=remotebuildexecution.googleapis.com`
 * `RECC_REMOTE_PLATFORM_CONTAINER_IMAGE=docker://gcr.io/cloud-marketplace/...` The Docker image from google's cloud registry for the worker to run in

You will also need to be authenticated with GCP, which can happen several ways. See https://cloud.google.com/docs/authentication/
for instructions on how to do that.


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

You can also optionally set `RECC_JOBS_COUNT` to the maximum number of jobs
each worker can execute before terminating execution. This can be useful when
running reccworker on Kubernetes (and similar platforms) since it can provide a 
"free sandbox" with `RECC_JOBS_COUNT=1`  with Kubernetes' (or equivalent) 
`restartPolicy: Always`.

Once you've set up the servers and environment, start the worker by
running `reccworker`. You can optionally specify a bot ID (the default is the
computer's hostname and the pid of the process separated by a '-') by passing
it as an argument.

## Additional utilities

This repo includes a couple of additional utilities that may be useful for
debugging. They aren't installed by default, but running `make` will place
them in the `bin` subdirectory of the project root.

- `deps [command]` - Print the names of the files needed to run the given
  command. (`recc` uses this to decide which files to send to the Remote
  Execution server.)

- `casupload [files]` - Upload the given files to CAS, then print the digest
  hash and size of the resulting Directory message.
