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

#ifndef INCLUDED_ENV
#define INCLUDED_ENV

#include <deque>
#include <map>
#include <set>
#include <string>

namespace BloombergLP {
namespace recc {

/**
 * The URI of the server to use, e.g. localhost:8085
 */
extern std::string RECC_SERVER;

/**
 * The URI of the CAS server to use. By default, uses RECC_SERVER.
 */
extern std::string RECC_CAS_SERVER;

/**
 * The instance name to pass to the server. The default is the empty string.
 */
extern std::string RECC_INSTANCE;

/**
 * If set, the contents of this directory (and its subdirectories) will be sent
 * to the worker.
 *
 * If both this and RECC_DEPS_OVERRIDE are set, the directory value is used.
 */
extern std::string RECC_DEPS_DIRECTORY_OVERRIDE;

/**
 * The location to store temporary files. (Currently used only by the worker.)
 */
extern std::string TMPDIR;

/**
 * Enables verbose output, which is logged to stderr.
 */
extern bool RECC_VERBOSE;

/**
 * Sends the command to the build server, even if deps doesn't think it's a
 * compiler command.
 */
extern bool RECC_FORCE_REMOTE;

/**
 * Sets the `do_not_cache` flag in the Action to indicate that it can never be
 * cached.
 */
extern bool RECC_ACTION_UNCACHEABLE;

/**
 * Sets the `skip_cache_lookup` flag in the ExecuteRequest to re-run the action
 * instead of looking it up in the cache.
 */
extern bool RECC_SKIP_CACHE;

/**
 * Prevents compilation output from being saved to disk.
 */
extern bool RECC_DONT_SAVE_OUTPUT;

/**
 * Use Google's authentication to talk to the build server. Also applies to the
 * CAS server. Not setting this implies insecure communication.
 */
extern bool RECC_SERVER_AUTH_GOOGLEAPI;

/**
 * The maximum number of execution jobs to run concurrently. (Used only by the
 * worker.)
 */
extern int RECC_MAX_CONCURRENT_JOBS;

/*
 * The maximum number of jobs to run before terminating the worker. (Used only
 * by the worker.)
 */
extern int RECC_JOBS_COUNT;

/**
 * Defined by cmake when building.
 * On linux defaults to: /usr/local
 */
extern std::string RECC_INSTALL_DIR;

/**
 * A comma-separated list of input file paths to send to the build server. If
 * this isn't set, deps is called to determine the input files.
 */
extern std::set<std::string> RECC_DEPS_OVERRIDE;

/**
 * A comma-separated list of output file paths to request from the build
 * server. If this isn't set, deps attempts to guess the output files by
 * examining the command.
 */
extern std::set<std::string> RECC_OUTPUT_FILES_OVERRIDE;

/**
 * A comma-separated list of output directories to request from the build
 * server. If this is set, deps isn't called.
 */
extern std::set<std::string> RECC_OUTPUT_DIRECTORIES_OVERRIDE;

// Maps are given by passing an environment variable for each item in the map.
// For example, RECC_REMOTE_ENV_PATH=/usr/bin could be used to specify the PATH
// passed to the remote build server.

/**
 * Environment variables to apply to dependency commands, which are run on the
 * local machine.
 */
extern std::map<std::string, std::string> RECC_DEPS_ENV;

/**
 * Environment variables to send to the build server. For example, you can set
 * the remote server's PATH using RECC_REMOTE_ENV_PATH=/usr/bin
 */
extern std::map<std::string, std::string> RECC_REMOTE_ENV;

/**
 * Platform requirements to send to the build server. For example, you can
 * require x86_64 using RECC_REMOTE_PLATFORM_arch=x86_64
 */
extern std::map<std::string, std::string> RECC_REMOTE_PLATFORM;

/**
 * Used to specify absolute paths for finding recc.conf.
 * If specifing absolute path, only include up until directory containing
 * config, no trailing "/". Additions to the list should be in order of
 * priority: LEAST-> MOST important.
 *
 * Additional locations based on the runtime environment are included in:
 * env.cpp:find_and_parse_config_files()
 */
extern std::deque<std::string> RECC_CONFIG_LOCATIONS;

/**
 * Parse the given environment and store it in the corresponding global
 * variables.
 *
 * environ should be an array of "VARIABLE=value" strings whose last entry is
 * nullptr.
 */
void parse_config_variables(const char *const *environ);

/**
 * Finds config files specified in RECC_CONFIG_LOCATIONS and passes variables
 * to parse_config_variables
 */
void find_and_parse_config_files();

/**
 * Handles the case that RECC_SERVER and RECC_CAS_SERVER have not been set.
 */
void handle_special_defaults();

/*
 * Append default location to look for recc.conf files by default looks in
 * these locations:
 *
 * 1. ${cwd}/recc/recc.conf
 * 2. ${HOME}/.recc/recc.conf
 * 3. ${INSTALL_DIR}/../etc/recc/recc.conf
 */
void add_default_locations();

/**
 * The process environment.
 */
extern "C" char **environ;

/**
 * Add default recc locations, parse the config files, then the
 * environment, and then checks whether important fields are empty.
 */
inline void parse_config_variables()
{
    add_default_locations();
    find_and_parse_config_files();
    parse_config_variables(environ);
    handle_special_defaults();
}
} // namespace recc
} // namespace BloombergLP

#endif
