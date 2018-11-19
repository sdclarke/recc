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

#include <algorithm>

#include <env.h>

#include <cstring>

#include <ctype.h>

#include <fstream>

#include <iostream>

#include <reccdefaults.h>

#include <unistd.h>

#include <logging.h>

#include <vector>

#include <sstream>

#include <string>

using namespace std;

namespace BloombergLP {
namespace recc {

// Leave these empty so that parse_config_variables can print warnings if not
// specified
string RECC_SERVER = "";
string RECC_CAS_SERVER = "";

// Include default values for the following, no need to print warnings if not
// specified
string RECC_INSTANCE = DEFAULT_RECC_INSTANCE;
string RECC_DEPS_DIRECTORY_OVERRIDE = DEFAULT_RECC_DEPS_DIRECTORY_OVERRIDE;
string TMPDIR = DEFAULT_RECC_TMPDIR;

bool RECC_VERBOSE = DEFAULT_RECC_VERBOSE;
bool RECC_FORCE_REMOTE = DEFAULT_RECC_FORCE_REMOTE;
bool RECC_ACTION_UNCACHEABLE = DEFAULT_RECC_ACTION_UNCACHEABLE;
bool RECC_SKIP_CACHE = DEFAULT_RECC_SKIP_CACHE;
bool RECC_DONT_SAVE_OUTPUT = DEFAULT_RECC_DONT_SAVE_OUTPUT;
bool RECC_SERVER_AUTH_GOOGLEAPI = DEFAULT_RECC_SERVER_AUTH_GOOGLEAPI;

int RECC_MAX_CONCURRENT_JOBS = DEFAULT_RECC_MAX_CONCURRENT_JOBS;
int RECC_JOBS_COUNT = DEFAULT_RECC_JOBS_COUNT;

#ifdef CMAKE_INSTALL_DIR
string RECC_INSTALL_DIR = string(CMAKE_INSTALL_DIR);
#else
string RECC_INSTALL_DIR = string("");
#endif

set<string> RECC_DEPS_OVERRIDE = DEFAULT_RECC_DEPS_OVERRIDE;
set<string> RECC_OUTPUT_FILES_OVERRIDE = DEFAULT_RECC_OUTPUT_FILES_OVERRIDE;
set<string> RECC_OUTPUT_DIRECTORIES_OVERRIDE =
    DEFAULT_RECC_OUTPUT_DIRECTORIES_OVERRIDE;

map<string, string> RECC_DEPS_ENV = DEFAULT_RECC_DEPS_ENV;
map<string, string> RECC_REMOTE_ENV = DEFAULT_RECC_REMOTE_ENV;
map<string, string> RECC_REMOTE_PLATFORM = DEFAULT_RECC_REMOTE_PLATFORM;

deque<string> RECC_CONFIG_LOCATIONS = DEFAULT_RECC_CONFIG_LOCATIONS;

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

/**
 * Formats line to be used in parse_config_variables.
 * Modifies parameter passed to it by reference
 */
void format_config_string(string &line)
{
    // remove whitespace
    line.erase(remove(line.begin(), line.end(), ' '), line.end());

    // converts only the KEY to uppercase
    transform(line.begin(), line.begin() + line.find('='), line.begin(),
              [](unsigned char c) -> unsigned char { return toupper(c); });

    // prefix "RECC_" to name, unless name is TMPDIR
    if (line.substr(0, 6) != "TMPDIR") {
        line = "RECC_" + line;
    }
}

/*
 * Parse the config variables, and pass to parse_config_variables
 */
void parse_config_files(const string &config_file_name)
{
    ifstream config(config_file_name);
    string line;
    vector<string> env_array;
    vector<char *> env_cstrings;

    while (getline(config, line)) {
        if (line.empty() || isspace(line[0]) || line[0] == '#') {
            continue;
        }
        format_config_string(line);
        env_array.push_back(line);
    }
    // first push strings into vector, then push_back char *
    // done for easy const char** conversion
    for (string &i : env_array) {
        env_cstrings.push_back(const_cast<char *>(i.c_str()));
    }
    env_cstrings.push_back(nullptr);
    parse_config_variables(env_cstrings.data());
}

void parse_config_variables(const char *const *environ)
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

    // Parse all the options from ENV
    for (int i = 0; environ[i] != nullptr; ++i) {
        VARS_START()
        STRVAR(RECC_SERVER)
        STRVAR(RECC_CAS_SERVER)
        STRVAR(RECC_INSTANCE)
        STRVAR(RECC_DEPS_DIRECTORY_OVERRIDE)
        STRVAR(TMPDIR)

        BOOLVAR(RECC_VERBOSE)
        BOOLVAR(RECC_FORCE_REMOTE)
        BOOLVAR(RECC_ACTION_UNCACHEABLE)
        BOOLVAR(RECC_SKIP_CACHE)
        BOOLVAR(RECC_DONT_SAVE_OUTPUT)
        BOOLVAR(RECC_SERVER_AUTH_GOOGLEAPI)

        INTVAR(RECC_MAX_CONCURRENT_JOBS)
        INTVAR(RECC_JOBS_COUNT)

        SETVAR(RECC_DEPS_OVERRIDE)
        SETVAR(RECC_OUTPUT_FILES_OVERRIDE)
        SETVAR(RECC_OUTPUT_DIRECTORIES_OVERRIDE)

        MAPVAR(RECC_DEPS_ENV)
        MAPVAR(RECC_REMOTE_ENV)
        MAPVAR(RECC_REMOTE_PLATFORM)
    }
}

void find_and_parse_config_files()
{
    for (auto file_location : RECC_CONFIG_LOCATIONS) {
        ifstream config(file_location);
        if (config.good()) {
            // append name of config file, defined by DEFAULT_RECC_CONFIG
            file_location = file_location + "/" + DEFAULT_RECC_CONFIG;
            RECC_LOG_VERBOSE("Found recc config at: " << file_location);
            parse_config_files(file_location);
        }
    }
}

void handle_special_defaults()
{
    if (RECC_SERVER.empty()) {
        RECC_SERVER = DEFAULT_RECC_SERVER;
        RECC_LOG_WARNING(
            "Warning: no RECC_SERVER environment variable specified."
            << "Using default server (" << RECC_SERVER << ")");
    }

    if (RECC_CAS_SERVER.empty()) {
        RECC_CAS_SERVER = RECC_SERVER;
        RECC_LOG_WARNING("Warning: no RECC_CAS_SERVER environment variable "
                         "specified."
                         << "Using the same as RECC_SERVER ("
                         << RECC_CAS_SERVER << ")");
    }
}

void add_default_locations()
{
    string home = getenv("HOME");
    const string cwd_recc = "./recc";

    // Note that the order in which the config locations are pushed
    // is significant.

    RECC_CONFIG_LOCATIONS.push_front(cwd_recc);

    if (!home.empty()) {
        home += "/.recc";
        RECC_CONFIG_LOCATIONS.push_front(home);
    }

    if (!RECC_INSTALL_DIR.empty()) {
        RECC_INSTALL_DIR += "/../etc/recc";
        RECC_CONFIG_LOCATIONS.push_front(RECC_INSTALL_DIR);
    }
}

} // namespace recc
} // namespace BloombergLP
