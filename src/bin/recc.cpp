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

// bin/recc.cpp
//
// Runs a build command remotely. If the given command is not a build command,
// it's actually run locally.

#include <actionbuilder.h>
#include <authsession.h>
#include <deps.h>
#include <env.h>
#include <fileutils.h>
#include <formpost.h>
#include <grpcchannels.h>
#include <grpccontext.h>
#include <logging.h>
#include <merklize.h>
#include <metricsconfig.h>
#include <reccdefaults.h>
#include <remoteexecutionclient.h>
#include <requestmetadata.h>

#include <cstdio>
#include <cstring>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

#include <reccmetrics/durationmetrictimer.h>
#include <reccmetrics/durationmetricvalue.h>
#include <reccmetrics/metricguard.h>
#include <reccmetrics/publisherguard.h>
#include <reccmetrics/statsdpublisher.h>
#include <reccmetrics/totaldurationmetricvalue.h>

#define TIMER_NAME_EXECUTE_ACTION "recc.execute_action"
#define TIMER_NAME_QUERY_ACTION_CACHE "recc.query_action_cache"

using namespace BloombergLP::recc;

/**
 * NOTE: If a variable is intended to be used in a configuration file, omit the
 * "RECC_" prefix.
 */
const std::string HELP(
    "USAGE: recc <command>\n"
    "\n"
    "If the given command is a compile command, runs it on a remote build\n"
    "server. Otherwise, runs it locally.\n"
    "\n"
    "The following environment variables can be used to change recc's\n"
    "behavior. To set them in a recc.conf file, omit the \"RECC_\" prefix.\n"
    "\n"
    "RECC_SERVER - the URI of the server to use (e.g. localhost:8085)\n"
    "\n"
    "RECC_CAS_SERVER - the URI of the CAS server to use (by default, \n"
    "                  use RECC_ACTION_CACHE_SERVER if set. Else "
    "RECC_SERVER)\n"
    "\n"
    "RECC_ACTION_CACHE_SERVER - the URI of the Action Cache server to use (by "
    "default,\n"
    "                  use RECC_CAS_SERVER. Else RECC_SERVER)\n"
    "\n"
    "RECC_PROJECT_ROOT - the top-level directory of the project source.\n"
    "                    If the command contains paths inside the root, they\n"
    "                    will be rewritten to relative paths (by default, \n"
    "                    use the current working directory)\n"
    "\n"
    "RECC_SERVER_AUTH_GOOGLEAPI - use default google authentication when\n"
    "                             communicating over gRPC, instead of\n"
    "                             using an insecure connection\n"
    "\n"
    "RECC_SERVER_SSL - use a secure SSL/TLS channel when communicating over\n"
    "                  grpc\n"
    "\n"
    "RECC_SERVER_JWT - use a secure SSL/TLS channel, and authenticate with "
    "JWT\n"
    "                  when communicating with the execution and cas servers\n"
    "\n"
    "RECC_JWT_JSON_FILE_PATH - path specifying location of JWT access token.\n"
    "                          defaults to " DEFAULT_RECC_JWT_JSON_FILE_PATH
    " (JSON format expected)\n"
    "\n"
    "RECC_AUTH_REFRESH_URL - url specifying location to refresh access "
    "tokens.\n"
    "                        If empty string, will try to refresh token in "
    "memory, using token on disk/file. \n"
    "                        Defaults to empty string. Expects server to "
    "return access "
    "and refresh\n"
    "                        tokens (in serialized JSON) when provided a "
    "refresh\n"
    "                        token (in serialized JSON)\n"
    "\n"
    "RECC_INSTANCE - the instance name to pass to the server\n"
    "\n"
    "RECC_VERBOSE - enable verbose output\n"
    "\n"
    "RECC_ENABLE_METRICS - enable metric collection (Defaults to False)\n"
    "\n"
    "RECC_METRICS_FILE - write metrics to that file (Default/Empty string — "
    "stderr). Cannot be used with RECC_METRICS_UDP_SERVER.\n"
    "\n"
    "RECC_METRICS_UDP_SERVER - write metrics to the specified host:UDP_Port\n"
    " Cannot be used with RECC_METRICS_FILE\n"
    "\n"
    "RECC_FORCE_REMOTE - send all commands to the build server. (Non-compile\n"
    "                    commands won't be executed locally, which can cause\n"
    "                    some builds to fail.)\n"
    "\n"
    "RECC_ACTION_UNCACHEABLE - sets `do_not_cache` flag to indicate that\n"
    "                          the build action can never be cached\n"
    "\n"
    "RECC_SKIP_CACHE - sets `skip_cache_lookup` flag to re-run the build\n"
    "                  action instead of looking it up in the cache\n"
    "\n"
    "RECC_DONT_SAVE_OUTPUT - prevent build output from being saved to\n"
    "                        local disk\n"
    "\n"
    "RECC_DEPS_GLOBAL_PATHS - report all entries returned by the dependency\n"
    "                         command, even if they are absolute paths\n"
    "\n"
    "RECC_DEPS_OVERRIDE - comma-separated list of files to send to the\n"
    "                     build server (by default, run `deps` to\n"
    "                     determine this)\n"
    "\n"
    "RECC_DEPS_DIRECTORY_OVERRIDE - directory to send to the build server\n"
    "                               (if both this and RECC_DEPS_OVERRIDE\n"
    "                               are set, this one is used)\n"
    "\n"
    "RECC_OUTPUT_FILES_OVERRIDE - comma-separated list of files to\n"
    "                             request from the build server (by\n"
    "                             default, `deps` guesses)\n"
    "\n"
    "RECC_OUTPUT_DIRECTORIES_OVERRIDE - comma-separated list of\n"
    "                                   directories to request (by\n"
    "                                   default, `deps` guesses)\n"
    "\n"
    "RECC_DEPS_ENV_[var] - sets [var] for local dependency detection\n"
    "                      commands\n"
    "\n"
    "RECC_REMOTE_ENV_[var] - sets [var] in the remote build environment\n"
    "\n"
    "RECC_REMOTE_PLATFORM_[key] - specifies required Platform property,\n"
    "                             which the build server uses to select\n"
    "                             the build worker\n"
    "\n"
    "RECC_RETRY_LIMIT - number of times to retry failed requests (default "
    "0).\n"
    "			Recc will try to refresh once on unauthenticated "
    "errors\n"
    "			if RECC_AUTH_REFRESH_URL is set. This will not count\n"
    "			towards the limit\n"
    "\n"
    "RECC_RETRY_DELAY - base delay (in ms) between retries\n"
    "                   grows exponentially (default 100ms)");

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        RECC_LOG_ERROR("USAGE: recc <command>");
        RECC_LOG_ERROR("(run \"recc --help\" for details)");
        return 1;
    }
    else if (argc == 2 &&
             (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        RECC_LOG_WARNING(HELP);
        return 0;
    }
    else if (argc == 2 && (strcmp(argv[1], "--version") == 0 ||
                           strcmp(argv[1], "-v") == 0)) {
        const std::string version =
            RequestMetadataGenerator::RECC_METADATA_TOOL_VERSION;
        const std::string versionMessage = "recc version: " + version;
        RECC_LOG_WARNING(versionMessage);
        return 0;
    }

    set_config_locations();
    parse_config_variables();

    reccmetrics::PublisherGuard<StatsDPublisherType> statsDPublisherGuard(
        RECC_ENABLE_METRICS, get_statsdpublisher_from_config());

    const std::string cwd = get_current_working_directory();
    ParsedCommand command(&argv[1], cwd.c_str());

    std::unordered_map<proto::Digest, std::string> blobs;
    std::unordered_map<proto::Digest, std::string> filenames;

    std::shared_ptr<proto::Action> actionPtr;

    try {
        actionPtr =
            ActionBuilder::BuildAction(command, cwd, &blobs, &filenames);
    }
    catch (const subprocess_failed_error &e) {
        exit(e.d_error_code);
    }

    if (!actionPtr) {
        execvp(argv[1], &argv[1]);
        RECC_LOG_PERROR(argv[1]);
        exit(1);
    }
    auto action = *actionPtr;
    auto actionDigest = make_digest(action);
    RECC_LOG_VERBOSE("Action Digest: " << actionDigest.ShortDebugString()
                                       << " Action Contents: "
                                       << action.ShortDebugString());

    int rc = -1;
    try {
        GrpcChannels returnChannels = GrpcChannels::get_channels_from_config();

        GrpcContext grpcContext;
        grpcContext.set_action_id(actionDigest.hash());

        std::unique_ptr<AuthSession> reccAuthSession;
        FormPost formPostFactory;
        if (RECC_SERVER_JWT) {
            reccAuthSession.reset(new AuthSession(&formPostFactory));
            grpcContext.set_auth(reccAuthSession.get());
        }
        RemoteExecutionClient client(
            returnChannels.server(), returnChannels.cas(),
            returnChannels.action_cache(), RECC_INSTANCE, &grpcContext);

        ActionResult result;

        /* If allowed, we look in the action cache first */
        bool action_in_cache = false;
        if (!RECC_SKIP_CACHE) {
            try {
                { // Timed block
                    reccmetrics::MetricGuard<reccmetrics::DurationMetricTimer>
                        mt(TIMER_NAME_QUERY_ACTION_CACHE, RECC_ENABLE_METRICS);

                    action_in_cache = client.fetch_from_action_cache(
                        actionDigest, RECC_INSTANCE, result);
                    if (action_in_cache) {
                        RECC_LOG_VERBOSE("Action cache hit for "
                                         << actionDigest.hash());
                    }
                }
            }
            catch (const std::runtime_error &e) {
                RECC_LOG_VERBOSE(e.what());
            }
        }

        if (!action_in_cache) {
            blobs[actionDigest] = action.SerializeAsString();
            RECC_LOG_VERBOSE("Uploading resources...");
            client.upload_resources(blobs, filenames);
            RECC_LOG_VERBOSE("Executing action...");
            { // Timed block
                reccmetrics::MetricGuard<reccmetrics::DurationMetricTimer> mt(
                    TIMER_NAME_EXECUTE_ACTION, RECC_ENABLE_METRICS);

                result = client.execute_action(actionDigest, RECC_SKIP_CACHE);
            }
        }

        rc = result.d_exitCode;

        /* These don't use logging macros because they are compiler output */
        std::cout << client.get_outputblob(result.d_stdOut);
        std::cerr << client.get_outputblob(result.d_stdErr);

        if (!RECC_DONT_SAVE_OUTPUT) {
            client.write_files_to_disk(result);
        }
    }
    catch (const std::exception &e) {
        RECC_LOG_ERROR(e.what());
        rc = (rc == 0 ? -1 : rc);
    }
    return rc;
}
