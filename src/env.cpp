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

#include <fileutils.h>
#include <logging.h>
#include <reccdefaults.h>

#include <algorithm>
#include <cstring>
#include <ctype.h>
#include <env.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace BloombergLP {
namespace recc {

// Leave these empty so that parse_config_variables can print warnings if not
// specified
std::string RECC_SERVER = "";
std::string RECC_CAS_SERVER = "";
std::string RECC_ACTION_CACHE_SERVER = "";

// Include default values for the following, no need to print warnings if not
// specified
std::string RECC_INSTANCE = DEFAULT_RECC_INSTANCE;
std::string RECC_DEPS_DIRECTORY_OVERRIDE =
    DEFAULT_RECC_DEPS_DIRECTORY_OVERRIDE;
std::string RECC_PROJECT_ROOT = DEFAULT_RECC_PROJECT_ROOT;
std::string TMPDIR = DEFAULT_RECC_TMPDIR;
std::string RECC_JWT_JSON_FILE_PATH = DEFAULT_RECC_JWT_JSON_FILE_PATH;
std::string RECC_AUTH_REFRESH_URL = DEFAULT_RECC_AUTH_REFRESH_URL;
std::string RECC_CORRELATED_INVOCATIONS_ID =
    DEFAULT_RECC_CORRELATED_INVOCATIONS_ID;
std::string RECC_METRICS_FILE = DEFAULT_RECC_METRICS_FILE;
std::string RECC_METRICS_UDP_SERVER = DEFAULT_RECC_METRICS_UDP_SERVER;

bool RECC_VERBOSE = DEFAULT_RECC_VERBOSE;
bool RECC_ENABLE_METRICS = DEFAULT_RECC_ENABLE_METRICS;
bool RECC_FORCE_REMOTE = DEFAULT_RECC_FORCE_REMOTE;
bool RECC_ACTION_UNCACHEABLE = DEFAULT_RECC_ACTION_UNCACHEABLE;
bool RECC_SKIP_CACHE = DEFAULT_RECC_SKIP_CACHE;
bool RECC_DONT_SAVE_OUTPUT = DEFAULT_RECC_DONT_SAVE_OUTPUT;
bool RECC_SERVER_AUTH_GOOGLEAPI = DEFAULT_RECC_SERVER_AUTH_GOOGLEAPI;
bool RECC_SERVER_SSL = DEFAULT_RECC_SERVER_SSL;
bool RECC_SERVER_JWT = DEFAULT_RECC_SERVER_JWT;
bool RECC_DEPS_GLOBAL_PATHS = DEFAULT_RECC_DEPS_GLOBAL_PATHS;

int RECC_RETRY_LIMIT = DEFAULT_RECC_RETRY_LIMIT;
int RECC_RETRY_DELAY = DEFAULT_RECC_RETRY_DELAY;

// Hidden variables (not displayed in the help string)
std::string RECC_AUTH_UNCONFIGURED_MSG = DEFAULT_RECC_AUTH_UNCONFIGURED_MSG;

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
        const auto comma = strchr(str, ',');
        if (comma == nullptr) {
            result->insert(std::string(str));
            return;
        }
        else {
            result->insert(std::string(str, static_cast<size_t>(comma - str)));
            str = comma + 1;
        }
    }
}

/**
 * Formats line to be used in parse_config_variables.
 * Modifies parameter passed to it by pointer
 */
void format_config_string(std::string *line)
{
    // converts only the KEY to uppercase
    transform(line->cbegin(),
              line->cbegin() +
                  static_cast<std::string::difference_type>(line->find('=')),
              line->begin(), ::toupper);

    const std::string tmpdirConfig = "TMPDIR";

    // prefix "RECC_" to name, unless name is TMPDIR
    if (line->substr(0, tmpdirConfig.length()) != tmpdirConfig) {
        *line = "RECC_" + *line;
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
        format_config_string(&line);
        env_array.push_back(line);
    }
    // first push std::strings into vector, then push_back char *
    // done for easy const char** conversion
    for (const std::string &i : env_array) {
        env_cstrings.push_back(const_cast<char *>(i.c_str()));
    }
    env_cstrings.push_back(nullptr);
    parse_config_variables(env_cstrings.data());
}

void parse_config_variables(const char *const *env)
{
#define VARS_START()                                                          \
    if (strncmp(env[i], "RECC_", 4) != 0 &&                                   \
        strncmp(env[i], "TMPDIR", 6) != 0) {                                  \
        continue;                                                             \
    }
#define STRVAR(name)                                                          \
    else if (strncmp(env[i], #name "=", strlen(#name "=")) == 0)              \
    {                                                                         \
        name = std::string(env[i] + strlen(#name "="));                       \
    }
#define BOOLVAR(name)                                                         \
    else if (strncmp(env[i], #name "=", strlen(#name "=")) == 0)              \
    {                                                                         \
        name = strlen(env[i]) > strlen(#name "=");                            \
    }
#define INTVAR(name)                                                          \
    else if (strncmp(env[i], #name "=", strlen(#name "=")) == 0)              \
    {                                                                         \
        name = std::stoi(std::string(env[i] + strlen(#name "=")));            \
    }
#define SETVAR(name)                                                          \
    else if (strncmp(env[i], #name "=", strlen(#name "=")) == 0)              \
    {                                                                         \
        parse_set(env[i] + strlen(#name "="), &name);                         \
    }
#define MAPVAR(name)                                                          \
    else if (strncmp(env[i], #name "_", strlen(#name "_")) == 0)              \
    {                                                                         \
        const char *equals = strchr(env[i], '=');                             \
        std::string key(env[i] + strlen(#name "_"),                           \
                        equals - env[i] - strlen(#name "_"));                 \
        name[key] = std::string(equals + 1);                                  \
    }

    // Parse all the options from ENV
    for (int i = 0; env[i] != nullptr; ++i) {
        VARS_START()
        STRVAR(RECC_SERVER)
        STRVAR(RECC_CAS_SERVER)
        STRVAR(RECC_ACTION_CACHE_SERVER)
        STRVAR(RECC_INSTANCE)
        STRVAR(RECC_DEPS_DIRECTORY_OVERRIDE)
        STRVAR(RECC_PROJECT_ROOT)
        STRVAR(TMPDIR)
        STRVAR(RECC_JWT_JSON_FILE_PATH)
        STRVAR(RECC_AUTH_UNCONFIGURED_MSG)
        STRVAR(RECC_AUTH_REFRESH_URL)
        STRVAR(RECC_CORRELATED_INVOCATIONS_ID)
        STRVAR(RECC_METRICS_FILE)
        STRVAR(RECC_METRICS_UDP_SERVER)

        BOOLVAR(RECC_VERBOSE)
        BOOLVAR(RECC_ENABLE_METRICS)
        BOOLVAR(RECC_FORCE_REMOTE)
        BOOLVAR(RECC_ACTION_UNCACHEABLE)
        BOOLVAR(RECC_SKIP_CACHE)
        BOOLVAR(RECC_DONT_SAVE_OUTPUT)
        BOOLVAR(RECC_SERVER_AUTH_GOOGLEAPI)
        BOOLVAR(RECC_SERVER_SSL)
        BOOLVAR(RECC_SERVER_JWT)
        BOOLVAR(RECC_DEPS_GLOBAL_PATHS)

        INTVAR(RECC_RETRY_LIMIT)
        INTVAR(RECC_RETRY_DELAY)

        SETVAR(RECC_DEPS_OVERRIDE)
        SETVAR(RECC_OUTPUT_FILES_OVERRIDE)
        SETVAR(RECC_OUTPUT_DIRECTORIES_OVERRIDE)

        MAPVAR(RECC_DEPS_ENV)
        MAPVAR(RECC_REMOTE_ENV)
        MAPVAR(RECC_REMOTE_PLATFORM)
    }
} // namespace recc

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

void handle_special_defaults()
{
    if (RECC_SERVER.empty()) {
        RECC_SERVER = DEFAULT_RECC_SERVER;
        RECC_LOG_WARNING("Warning: no RECC_SERVER environment variable "
                         "specified."
                         << " Using default server (" << RECC_SERVER << ")");
    }

    if (RECC_CAS_SERVER.empty()) {
        if (RECC_ACTION_CACHE_SERVER.empty()) {
            RECC_CAS_SERVER = RECC_SERVER;
            RECC_LOG_VERBOSE("No RECC_CAS_SERVER environment variable "
                             "specified."
                             << " Using the same as RECC_SERVER ("
                             << RECC_CAS_SERVER << ")");
        }
        else {
            // Since it makes most sense for the action cache and the CAS to
            // live together rather than the CAS living with the Execution
            // Service, using the AC endpoint.
            RECC_CAS_SERVER = RECC_ACTION_CACHE_SERVER;
            RECC_LOG_VERBOSE(
                "No RECC_CAS_SERVER environment variable specified. Using the "
                "same RECC_ACTION_CACHE_SERVER ("
                << RECC_ACTION_CACHE_SERVER << ")");
        }
    }
    if (RECC_ACTION_CACHE_SERVER.empty()) {
        RECC_ACTION_CACHE_SERVER = RECC_CAS_SERVER;
        RECC_LOG_VERBOSE("No RECC_ACTION_CACHE_SERVER environment variable "
                         "specified."
                         << " Using the same as RECC_CAS_SERVER ("
                         << RECC_CAS_SERVER << ")");
    }

    if (!(RECC_SERVER_AUTH_GOOGLEAPI || RECC_SERVER_SSL || RECC_SERVER_JWT)) {
        if (!RECC_AUTH_UNCONFIGURED_MSG.empty()) {
            RECC_LOG_WARNING(RECC_AUTH_UNCONFIGURED_MSG);
        }
    }

    if (RECC_PROJECT_ROOT.empty()) {
        RECC_PROJECT_ROOT = get_current_working_directory();
        RECC_LOG_VERBOSE("No RECC_PROJECT_ROOT directory specified. "
                         << "Defaulting to current working directory ("
                         << RECC_PROJECT_ROOT << ")");
    }
    else if (RECC_PROJECT_ROOT.front() != '/') {
        RECC_PROJECT_ROOT = make_path_absolute(
            RECC_PROJECT_ROOT, get_current_working_directory());
        RECC_LOG_WARNING("Warning: RECC_PROJECT_ROOT was set to a relative "
                         "path. "
                         << "Rewriting to absolute path "
                         << RECC_PROJECT_ROOT);
    }

    if (RECC_METRICS_FILE.size() && RECC_METRICS_UDP_SERVER.size()) {
        throw std::runtime_error("You can either set RECC_METRICS_FILE or "
                                 "RECC_METRICS_UDP_SERVER, but not both.");
    }
}

void verify_files_writeable()
{
    if (RECC_METRICS_FILE.size()) {
        std::ofstream of;
        of.open(RECC_METRICS_FILE, std::ofstream::out | std::ofstream::app);
        if (!of.good()) {
            throw std::runtime_error(
                "Cannot open RECC_METRICS_FILE for writing: " +
                RECC_METRICS_FILE);
        }
        of.close();
    }
    else if (RECC_METRICS_UDP_SERVER.size()) {
        std::string server_name;
        int server_port;
        try {
            parse_host_port_string(RECC_METRICS_UDP_SERVER, server_name,
                                   &server_port);
        }
        catch (const std::invalid_argument &e) {
            std::string error_msg =
                "Invalid RECC_METRICS_UDP_SERVER argument: '" +
                RECC_METRICS_UDP_SERVER + "': ";
            error_msg += e.what();
            throw std::runtime_error(error_msg);
        }
    }
}

std::deque<std::string> evaluate_config_locations()
{
    // Note that the order in which the config locations are pushed
    // is significant.
    std::deque<std::string> config_order;
    const std::string cwd_recc = "./recc";
    config_order.push_front(cwd_recc);

    const char *home = getenv("HOME");
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

void set_config_locations(const std::deque<std::string> &config_order)
{
    RECC_CONFIG_LOCATIONS = config_order;
}

void parse_host_port_string(const std::string &inputString,
                            std::string &serverRet, int *portRet)
{
    // NOTE: This only works for IPv4 addresses, not IPv6.
    const std::size_t split_at = inputString.find_last_of(":");

    if (split_at != std::string::npos &&
        split_at + 2 <
            inputString.size()) { // e.g. `localhost:1` or `example.org:1`
        try {
            *portRet = std::stoi(inputString.substr(split_at + 1));
            serverRet =
                inputString.substr(0, std::min(split_at, inputString.size()));
        }
        catch (const std::invalid_argument &) {
            throw std::invalid_argument(
                "Invalid port specified (cannot be parsed to int): '" +
                inputString.substr(split_at + 1) + "'");
        }
    }
    else { // e.g. `localhost` or `localhost:`
        // default to port 0 if not specified in the string
        *portRet = 0;
        serverRet =
            inputString.substr(0, std::min(split_at, inputString.size()));
    }
}

} // namespace recc
} // namespace BloombergLP
