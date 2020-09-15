# recc

`recc` is the Remote Execution Caching Compiler. It is a cross between `ccache` and `distcc` using the [Remote Execution APIs](remoteex).
When invoked with a C/C++ compilation command, it will use the APIs to communicate with a Remote Execution service (such as [BuildGrid][]) to
first determine whether the same build has been done before and is cached. If the build is cached remotely, download the result. Otherwise, enqueue
the build to be executed remotely using the same execution service which will then dispatch the job to a worker, such as [buildbox-worker][].

More information about `recc` and `BuildGrid` can be found in the [presentation][] we gave at Bazelcon 2018, and on this [blog post][].

`recc` is in active development and in use at [Bloomberg](techat). On Linux it primarily supports gcc; and on Solaris/Aix it supports vendor compilers.
Clang support is currently (very) experimental.

For information regarding contributing to this project, please read the [Contribution guide](CONTRIBUTING.md).

# Installation

## Dependencies
Currently recc relies on:

* [buildbox-common][]
* [gRPC][]
* [Protobuf][]
* [OpenSSL][]
* [CMake][]
* [GoogleTest][]
* [pkg-config][]

### Installing buildbox-common

Follow the instructions to install buildbox-common using the README located here: https://gitlab.com/BuildGrid/buildbox/buildbox-common

Once buildbox-common is installed, you should have the necessary dependencies to run recc. If not, please continue with a relevant section below.

<!-- # OSX -->

### Installing on OSX

Install [gRPC][], [Protobuf][], [OpenSSL][], [CMake][], [pkg-config][], through `brew`:

```sh
$ brew install grpc protobuf openssl cmake pkg-config
```

[GoogleTest][] is not available in Homebrew, so instead, you should download a copy of [its source code][googletest source], unzip it somewhere, and use `-DGTEST_SOURCE_ROOT` to tell CMake where to find it. (See below.)

You can now compile `recc`.

<!-- # Linux -->

### Installing on Debian/Ubuntu.

Install [OpenSSL][], [CMake][], [pkg-config][], [googletest][] through `apt`:

```sh
$ apt install cmake gcc g++ googletest libssl-dev pkg-config
```

**If not already installed**, install [gRPC][], [Protobuf][]:
```sh
$ apt install libgrpc++-dev libprotobuf-dev
```

Install the `googletest`, and `googlemock` binaries.

```sh
$ cd $(mktemp -d)
$ cmake /usr/src/googletest && make install
```

#### Debian 10/Ubuntu 18.10 or newer

Install [gRPC][], [Protobuf][]:

```sh
$ apt install protobuf-compiler-grpc libgrpc++-dev libprotobuf-dev
```

#### Debian 9/Ubuntu 18.04 or older

**Last tested with `grpc` version: 1.20.0 and `protoc` version: 3.8.0**

On these systems, the package versions of `protobuf` and `grpc` either don't exist or are too old for use with recc. Therefore manual build and install is necessary.
The instructions below for installing the `protobuf` compiler and `grpc`  are synthesized from the [grpc source](https://github.com/grpc/grpc/blob/master/BUILDING.md).

Install `c-ares` (the ubuntu package version does not include cmake
lists and therefore will not be used correctly).
```sh
$ cd $(mktemp -d)
$ apt install build-essential cmake curl
$ curl -OJL $(curl -s https://api.github.com/repos/c-ares/c-ares/releases/latest | grep browser_download_url | cut -d '"' -f 4 | head -n 1)
$ tar xf c-ares-*.tar.gz && rm c-ares-*.tar.gz
$ cd c-ares-*
$ mkdir build && cd build
$ cmake .. && make install
```

Clone and install the latest release of `protobuf` and `grpc`.
```sh
$ cd $(mktemp -d)
$ apt install autoconf libtool
$ git clone --recursive -b $(curl -L https://grpc.io/release) https://github.com/grpc/grpc
$ cd grpc/third_party/protobuf
$ ./autogen.sh
$ ./configure
$ make && make install
$ ldconfig
$ cd ../../
$
$ apt install libssl-dev zlib1g-dev
$ mkdir build && cd build
$ cmake -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DCMAKE_BUILD_TYPE=Release \
        ..
$ make install
```

Now you can follow the same instructions as the [Installing on Debian section](#installing-on-debian-based-linux-distributions) to install the other necessary dependencies, and build `recc`. Remember to skip the `protoc` and `grpc` install instructions!

<!-- # WSL -->

### Installing on WSL(Windows Subsystem for Linux).

From the Microsoft Store, download and open the `Debian` or `Ubuntu` applications.

Update the package lists, install the updated packages.

```sh
$ apt update && apt upgrade
```

Clone `recc` and follow the instructions in either the [Installing on Debian section](#installing-on-debian-based-linux-distributions) or [Installing on Ubuntu section](#installing-on-Ubuntu).


## Compiling
Once you've [installed the dependencies](#dependencies), you can compile `recc` using the following commands:

```sh
$ mkdir build
$ cd build
$ cmake .. && make
```

If you want to set the instance name at compile time, then you may pass in a special flag:

```sh
cmake -DDEFAULT_RECC_INSTANCE=name_you_want ../ && make
```

Note that on macOS, you'll need to manually specify the locations of OpenSSL and GoogleTest when running CMake:

```sh
$ cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DGTEST_SOURCE_ROOT=/wherever/you/unzipped/googletest/to .. && make
```

The optional flag `-DRECC_VERSION` allows setting the version string that recc
will attach as metadata to its request headers. If none is set, CMake will try
to determine the current git commit and use the short SHA as a version value.
(If that fails, the version will be set to "unknown".)

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

#### Running tests on macOS Mojave(10.14)

`/usr/include/`'s has been deprecated on Mojave, for certain tests to work, you must install Apple command line tools.

```sh
$ xcode-select --install
$ sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg -target /
```

### Compiling statically
You can compile recc statically with the `-DBUILD_STATIC=ON` option. All of recc's dependencies must be available as static libraries (`.a`files) and visible in `${CMAKE_MODULE_PATH}`.

## Running `recc`

`recc` is a command-line utility that runs compile commands on a Remote
Execution server. To run it, you'll first need to set up an execution server
for it to talk to. Then, set the appropriate configuration options and call
`recc` with a compile command.

### Setting up a Remote Execution server

`recc` should be compatible with any server that implements Bazel's
[Remote Execution API][] V2. It is primarily tested against [BuildGrid][],
so follow the instructions there to start a server.

The [Bazel Buildfarm][] project is also working on a Remote Execution server,
but at time of writing it uses version 1 of the API and so cannot be used with
`recc`.

#### Worker
You'll also need to set up a build worker to actually run the build jobs
you submit. BuildGrid provides one you can use.

This repository used to contain a reference worker called
`reccworker`, but that has since been deprecated and removed.
Some examples of workers are: [buildbox-worker](https://gitlab.com/BuildGrid/buildbox/buildbox-worker), [bgd-bot](https://gitlab.com/BuildGrid/buildgrid)

[remote execution api]: https://github.com/bazelbuild/remote-apis
[buildgrid]: https://gitlab.com/BuildGrid/buildgrid
[bazel buildfarm]: https://github.com/bazelbuild/bazel-buildfarm

### Configuration

`recc`’s default behavior can be overridden by configuration file settings,
which in turn can be overridden by environment variables with names starting
with `RECC_`. `recc` by default reads configuration options from the following
places, applying settings bottom-up, with 1 being the last applied
configuration (i.e. if an option is set in multiple files, the one higher on
this list will be the effective one):

  1) Environment variables

  2) `${cwd}/recc/recc.conf`

  3) `~/.recc/recc.conf`

  4) `${RECC_CONFIG_PREFIX_DIR}/recc.conf` when specified at compile time with `-DRECC_CONFIG_PREFIX_DIR=/path/to/custom/prefix`

  5) `${INSTALL_DIR}/../etc/recc/recc.conf`

#### Configuration File Syntax

Configuration files are in a simple “key=value” format, one setting per
line. Lines starting with a hash sign are comments. Blank lines are ignored.

At minimum, you'll need to set `RECC_SERVER` to the URI of your Remote:
```
  # set execution server to http://localhost:12345
  server=http://localhost:12345
```

Map configuration variables can be set similarly
```
  # set OS and compiler architecture targets for remote build.
  remote_platform_OSFamily=linux
  remote_platform_arch=x86_64
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

#### Support for dependency path replacement.

A common problem that can hinder reproducibility and cacheabilty of remote builds, are dependencies that are local to the user, system, and set of machines the build command is sent from. To solve this issue, `recc` supports specifying the `RECC_PREFIX_MAP` configuration variable, allowing changing a prefix in a path, with another one. For example, replacing all paths with prefixes including `/usr/local/bin` with `/usr/bin` can be done by specifying:
```sh
export RECC_PREFIX_MAP=/usr/local/bin=/usr/bin
```
Supports multiple prefixes by specifying `:` as a delimiter.

Guidelines for replacement are:
1. Both keys/values for RECC_PREFIX_MAP must be absolute paths.
2. Prefix candidates are matched from left to right, and once a match is found it is replaced and no more prefix replacement is done.
3. Path prefix replacement happens before absolute to relative path conversion.

#### Support for dependency filtering

When using `RECC_DEPS_GLOBAL_PATHS`, paths to system files (/usr/include, /opt/rh/devtoolset-7, etc) are included as part of the input root. To avoid these system dependencies potential conflicting with downstream build environment dependencies, there is now a method to filter out dependencies based on a set of paths. Setting the `RECC_DEPS_EXCLUDE_PATHS` environment variable with a comma-delimited set of paths(used as path prefixes) will be used as a filter to exclude those dependencies:
```sh
export RECC_DEPS_EXCLUDE_PATHS=/usr/include,/opt/rh/devtoolset-7
```

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
## CMake Integration

To integrate `recc` with CMake, replace/set these variables in your toolchain file.

```sh
set(CMAKE_C_COMPILER_LAUNCHER ${LOCATION_OF_RECC})
set(CMAKE_CXX_COMPILER_LAUNCHER ${LOCATION_OF_RECC})
```

Setting these flags will invoke recc, with the compiler and generated arguments.

## Additional utilities

This repo includes a couple of additional utilities that may be useful for
debugging. They aren't installed by default, but running `make` will place
them in the `bin` subdirectory of the project root.

- `deps [command]` - Print the names of the files needed to run the given
  command. (`recc` uses this to decide which files to send to the Remote
  Execution server.)

- `casupload [files]` - Upload the given files to CAS, then print the digest
  hash and size of the resulting Directory message.

<!-- Reference links -->
[buildbox-common]: https://gitlab.com/BuildGrid/buildbox/buildbox-common
[grpc]: https://grpc.io/
[protobuf]: https://github.com/google/protobuf/
[openssl]: https://www.openssl.org/
[cmake]: https://cmake.org/
[googletest]: https://github.com/google/googletest
[googletest source]: https://github.com/google/googletest/archive/release-1.8.1.zip
[pkg-config]: https://www.freedesktop.org/wiki/Software/pkg-config/
[remoteex]: https://docs.bazel.build/versions/master/remote-execution.html
[buildgrid]: http://buildgrid.build/
[buildbox-worker]: https://gitlab.com/BuildGrid/buildbox/buildbox-worker
[presentation]: https://www.youtube.com/watch?v=w1ZA4Rrf91I
[blog post]: https://www.codethink.com/articles/2018/introducing-buildgrid/
[techat]: https://www.techatbloomberg.com/
