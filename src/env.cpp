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
#include <unistd.h>
#include <logging.h>
#include <vector>
#include <sstream>
#include <string>

#include <reccdefaults.h>

namespace BloombergLP {
namespace recc {

// Leave these empty so that parse_config_variables can print warnings if not
// specified
std::string RECC_SERVER = "";
std::string RECC_CAS_SERVER = "";

// Include default values for the following, no need to print warnings if not
// specified
std::string RECC_INSTANCE = DEFAULT_RECC_INSTANCE;
std::string RECC_DEPS_DIRECTORY_OVERRIDE =
    DEFAULT_RECC_DEPS_DIRECTORY_OVERRIDE;
std::string TMPDIR = DEFAULT_RECC_TMPDIR;

bool RECC_VERBOSE = DEFAULT_RECC_VERBOSE;
bool RECC_FORCE_REMOTE = DEFAULT_RECC_FORCE_REMOTE;
bool RECC_ACTION_UNCACHEABLE = DEFAULT_RECC_ACTION_UNCACHEABLE;
bool RECC_SKIP_CACHE = DEFAULT_RECC_SKIP_CACHE;
bool RECC_DONT_SAVE_OUTPUT = DEFAULT_RECC_DONT_SAVE_OUTPUT;
bool RECC_SERVER_AUTH_GOOGLEAPI = DEFAULT_RECC_SERVER_AUTH_GOOGLEAPI;

int RECC_RETRY_LIMIT = DEFAULT_RECC_RETRY_LIMIT;
int RECC_RETRY_DELAY = DEFAULT_RECC_RETRY_DELAY;
int RECC_MAX_CONCURRENT_JOBS = DEFAULT_RECC_MAX_CONCURRENT_JOBS;
int RECC_JOBS_COUNT = DEFAULT_RECC_JOBS_COUNT;

#ifdef CMAKE_INSTALL_DIR
std::string RECC_INSTALL_DIR = std::string(CMAKE_INSTALL_DIR);
#else
std::string RECC_INSTALL_DIR = std::string("");
#endif

#ifdef RECC_CONFIG_PREFIX_DIR
std::string RECC_CUSTOM_PREFIX = std::string(RECC_CONFIG_PREFIX_DIR);
#else
std::string RECC_CUSTOM_PREFIX = std::string("");
#endif

std::set<std::string> RECC_DEPS_OVERRIDE = DEFAULT_RECC_DEPS_OVERRIDE;
std::set<std::string> RECC_OUTPUT_FILES_OVERRIDE =
    DEFAULT_RECC_OUTPUT_FILES_OVERRIDE;
std::set<std::string> RECC_OUTPUT_DIRECTORIES_OVERRIDE =
    DEFAULT_RECC_OUTPUT_DIRECTORIES_OVERRIDE;

std::map<std::string, std::string> RECC_DEPS_ENV = DEFAULT_RECC_DEPS_ENV;
std::map<std::string, std::string> RECC_REMOTE_ENV = DEFAULT_RECC_REMOTE_ENV;
std::map<std::string, std::string> RECC_REMOTE_PLATFORM =
    DEFAULT_RECC_REMOTE_PLATFORM;

// Keep this empty initially and have set_default_locations() populate it
std::deque<std::string> RECC_CONFIG_LOCATIONS = {};

/**
 * Parse a comma-separated list, storing its items in the given set.
 */
void parse_set(const char *str, std::set<std::string> *result)
{
    while (true) {
        auto comma = strchr(str, ',');
        if (comma == nullptr) {
            result->insert(std::string(str));
            return;
        }
        else {
            result->insert(std::string(str, comma - str));
            str = comma + 1;
        }
    }
}

/**
 * Formats line to be used in parse_config_variables.
 * Modifies parameter passed to it by reference
 */
void format_config_string(std::string &line)
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
void parse_config_files(const std::string &config_file_name)
{
    std::ifstream config(config_file_name);
    std::string line;
    std::vector<std::string> env_array;
    std::vector<char *> env_cstrings;

    while (getline(config, line)) {
        if (line.empty() || isspace(line[0]) || line[0] == '#') {
            continue;
        }
        format_config_string(line);
        env_array.push_back(line);
    }
    // first push std::strings into vector, then push_back char *
    // done for easy const char** conversion
    for (std::string &i : env_array) {
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
        name = std::string(environ[i] + strlen(#name "="));                   \
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
        std::string key(environ[i] + strlen(#name "_"),                       \
                        equals - environ[i] - strlen(#name "_"));             \
        name[key] = std::string(equals + 1);                                  \
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

        INTVAR(RECC_RETRY_LIMIT)
        INTVAR(RECC_RETRY_DELAY)
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
        std::ifstream config(file_location);
        if (config.good()) {
            // append name of config file, defined by DEFAULT_RECC_CONFIG
            file_location = file_location + "/" + DEFAULT_RECC_CONFIG;
            RECC_LOG_VERBOSE("Found recc config at: " << file_location);
            parse_config_files(file_location);
        }
    }
}

// defaults to Baseline
void handle_special_defaults(Source file)
{

    if (RECC_SERVER.empty()) {
        RECC_SERVER = DEFAULT_RECC_SERVER;
        RECC_LOG_WARNING("Warning: no RECC_SERVER environment variable "
                         "specified."
                         << "Using default server (" << RECC_SERVER << ")");
    }

    if (RECC_CAS_SERVER.empty()) {
        RECC_CAS_SERVER = RECC_SERVER;
        RECC_LOG_WARNING("Warning: no RECC_CAS_SERVER environment variable "
                         "specified."
                         << "Using the same as RECC_SERVER ("
                         << RECC_CAS_SERVER << ")");
    }

    if (file == Source::e_Reccworker) {
        if (RECC_MAX_CONCURRENT_JOBS <= 0) {
            RECC_LOG_WARNING(
                "Warning: no RECC_MAX_CONCURRENT_JOBS specified.");
            RECC_LOG_WARNING("Running "
                             << DEFAULT_RECC_MAX_CONCURRENT_JOBS
                             << " job(s) at a time (default option).");
            RECC_MAX_CONCURRENT_JOBS = DEFAULT_RECC_MAX_CONCURRENT_JOBS;
        }
        if (RECC_RETRY_LIMIT < 0) {
            RECC_LOG_WARNING("Warning: invalid RECC_RETRY_LIMIT setting.");
            RECC_LOG_WARNING("Retry limit set to " << DEFAULT_RECC_RETRY_LIMIT
                                                   << " (default value).");
            RECC_RETRY_LIMIT = DEFAULT_RECC_RETRY_LIMIT;
        }
        if (RECC_RETRY_DELAY < 0) {
            RECC_LOG_WARNING("Warning: invalid RECC_RETRY_DELAY setting.");
            RECC_LOG_WARNING("Retry delay set to " << DEFAULT_RECC_RETRY_DELAY
                                                   << "ms (default value).");
            RECC_RETRY_DELAY = DEFAULT_RECC_RETRY_DELAY;
        }
    }
}

std::deque<std::string> evaluate_config_locations()
{
    // Note that the order in which the config locations are pushed
    // is significant.
    std::deque<std::string> config_order;

    const char *home = getenv("HOME");
    const std::string cwd_recc = "./recc";

    config_order.push_front(cwd_recc);

    if (home != nullptr and home[0] != '\0') {
        config_order.push_front(home + std::string("/.recc"));
    }

    if (!RECC_CUSTOM_PREFIX.empty()) {
        config_order.push_front(RECC_CUSTOM_PREFIX);
    }

    if (!RECC_INSTALL_DIR.empty()) {
        RECC_INSTALL_DIR += "/../etc/recc";
        config_order.push_front(RECC_INSTALL_DIR);
    }

    return config_order;
}

void set_config_locations()
{
    set_config_locations(evaluate_config_locations());
}

void set_config_locations(std::deque<std::string> config_order)
{
    RECC_CONFIG_LOCATIONS = config_order;
}

} // namespace recc
} // namespace BloombergLP
