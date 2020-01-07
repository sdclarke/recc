// Copyright 2018 Bloomberg Finance L.P
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int dev_stdout_shim_fd(const char *pathname)
{
    if (strcmp(pathname, "/dev/stdin") == 0) {
        return STDIN_FILENO;
    }
    else if (strcmp(pathname, "/dev/stdout") == 0) {
        return STDOUT_FILENO;
    }
    else if (strcmp(pathname, "/dev/stderr") == 0) {
        return STDERR_FILENO;
    }
    return -1;
}

typedef int (*orig_open_fn)(const char *, int, ...);
int open(const char *pathname, int flags, ...)
{
    int fd = dev_stdout_shim_fd(pathname);
    if (fd != -1)
        return fd;
    orig_open_fn orig_open = (orig_open_fn)dlsym(RTLD_NEXT, "open");
    return orig_open(pathname, flags);
}

typedef int (*orig_creat_fn)(const char *, int);
int creat(const char *pathname, mode_t mode)
{
    int fd = dev_stdout_shim_fd(pathname);
    if (fd != -1)
        return fd;

    orig_creat_fn orig_creat = (orig_creat_fn)dlsym(RTLD_NEXT, "creat");
    return orig_creat(pathname, mode);
}

typedef FILE *(*orig_fopen_fn)(const char *, const char *);
FILE *fopen(const char *pathname, const char *type)
{
    int fd = dev_stdout_shim_fd(pathname);
    if (fd != -1)
        return fdopen(fd, type);

    orig_fopen_fn orig_fopen = (orig_fopen_fn)dlsym(RTLD_NEXT, "fopen");
    return orig_fopen(pathname, type);
}
