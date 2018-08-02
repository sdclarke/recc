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

#include <env.h>

#include <cstring>

using namespace std;

namespace BloombergLP {
namespace recc {

string RECC_SERVER;
string RECC_CAS_SERVER;
string RECC_INSTANCE;
string RECC_DEPS_OVERRIDE_DIRECTORY;
string TMPDIR("/tmp");

bool RECC_VERBOSE;
bool RECC_FORCE_REMOTE;
bool RECC_ACTION_UNCACHEABLE;
bool RECC_SKIP_CACHE;
bool RECC_DONT_SAVE_OUTPUT;

int RECC_MAX_CONCURRENT_JOBS = 0;

set<string> RECC_DEPS_OVERRIDE;
set<string> RECC_OUTPUT_FILES_OVERRIDE;
set<string> RECC_OUTPUT_DIRECTORIES_OVERRIDE;

#ifdef RECC_DEPS_ENV_DEFAULTS
map<string, string> RECC_DEPS_ENV = RECC_DEPS_ENV_DEFAULTS;
#else
map<string, string> RECC_DEPS_ENV;
#endif

#ifdef RECC_REMOTE_ENV_DEFAULTS
map<string, string> RECC_REMOTE_ENV = RECC_REMOTE_ENV_DEFAULTS;
#else
map<string, string> RECC_REMOTE_ENV;
#endif

#ifdef RECC_REMOTE_PLATFORM_DEFAULTS
map<string, string> RECC_REMOTE_PLATFORM = RECC_REMOTE_PLATFORM_DEFAULTS;
#else
map<string, string> RECC_REMOTE_PLATFORM;
#endif

/**
 * Parse a comma-separated list, storing its items in the given set.
 */
void parse_set(const char *str, set<string> *result)
{
    while (true) {
        auto comma = strchr(str, ',');
        if (comma == nullptr) {
            result->insert(string(str));
            return;
        }
        else {
            result->insert(string(str, comma - str));
            str = comma + 1;
        }
    }
}

void parse_environment(const char *const *environ)
{
#define VARS_START()                                                          \
    if (strncmp(environ[i], "RECC_", 4) != 0 &&                               \
        strncmp(environ[i], "TMPDIR", 6) != 0) {                              \
        continue;                                                             \
    }
#define STRVAR(name)                                                          \
    else if (strncmp(environ[i], #name "=", strlen(#name "=")) == 0)          \
    {                                                                         \
        name = string(environ[i] + strlen(#name "="));                        \
    }
#define BOOLVAR(name)                                                         \
    else if (strncmp(environ[i], #name "=", strlen(#name "=")) == 0)          \
    {                                                                         \
        name = strlen(environ[i]) > strlen(#name "=");                        \
    }
#define INTVAR(name)                                                          \
    else if (strncmp(environ[i], #name "=", strlen(#name "=")) == 0)          \
    {                                                                         \
        name = strtol(environ[i] + strlen(#name "="), nullptr, 10);           \
    }
#define SETVAR(name)                                                          \
    else if (strncmp(environ[i], #name "=", strlen(#name "=")) == 0)          \
    {                                                                         \
        parse_set(environ[i] + strlen(#name "="), &name);                     \
    }
#define MAPVAR(name)                                                          \
    else if (strncmp(environ[i], #name "_", strlen(#name "_")) == 0)          \
    {                                                                         \
        auto equals = strchr(environ[i], '=');                                \
        string key(environ[i] + strlen(#name "_"),                            \
                   equals - environ[i] - strlen(#name "_"));                  \
        name[key] = string(equals + 1);                                       \
    }

    for (int i = 0; environ[i] != nullptr; ++i) {
        VARS_START()
        STRVAR(RECC_SERVER)
        STRVAR(RECC_CAS_SERVER)
        STRVAR(RECC_INSTANCE)
        STRVAR(RECC_DEPS_OVERRIDE_DIRECTORY)
        STRVAR(TMPDIR)

        BOOLVAR(RECC_VERBOSE)
        BOOLVAR(RECC_FORCE_REMOTE)
        BOOLVAR(RECC_ACTION_UNCACHEABLE)
        BOOLVAR(RECC_SKIP_CACHE)
        BOOLVAR(RECC_DONT_SAVE_OUTPUT)

        INTVAR(RECC_MAX_CONCURRENT_JOBS)

        SETVAR(RECC_DEPS_OVERRIDE)
        SETVAR(RECC_OUTPUT_FILES_OVERRIDE)
        SETVAR(RECC_OUTPUT_DIRECTORIES_OVERRIDE)

        MAPVAR(RECC_DEPS_ENV)
        MAPVAR(RECC_REMOTE_ENV)
        MAPVAR(RECC_REMOTE_PLATFORM)
    }
}
} // namespace recc
} // namespace BloombergLP
