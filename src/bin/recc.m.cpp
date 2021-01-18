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
#include <deps.h>
#include <digestgenerator.h>
#include <env.h>
#include <fileutils.h>
#include <grpcchannels.h>
#include <grpccontext.h>
#include <metricsconfig.h>
#include <parsedcommandfactory.h>
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

#include <buildboxcommon_logging.h>
#include <buildboxcommonmetrics_durationmetrictimer.h>
#include <buildboxcommonmetrics_durationmetricvalue.h>
#include <buildboxcommonmetrics_metricguard.h>
#include <buildboxcommonmetrics_publisherguard.h>
#include <buildboxcommonmetrics_statsdpublisher.h>
#include <buildboxcommonmetrics_totaldurationmetricvalue.h>

#define TIMER_NAME_EXECUTE_ACTION "recc.execute_action"
#define TIMER_NAME_QUERY_ACTION_CACHE "recc.query_action_cache"

using namespace BloombergLP::recc;

namespace {

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
    "If the command is to be executed remotely, it must specify either a \n"
    "relative or absolute path to an executable.\n"
    "\n"
    "The following environment variables can be used to change recc's\n"
    "behavior. To set them in a recc.conf file, omit the \"RECC_\" prefix.\n"
    "\n"
    "RECC_SERVER - the URI of the server to use (e.g. http://localhost:8085)\n"
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
    "RECC_ACCESS_TOKEN_PATH - path specifying location of access token (JWT, "
    "OAuth, etc) to be attached to all secure connections.\n"
    "                         Defaults to \"" DEFAULT_RECC_ACCESS_TOKEN_PATH
    "\"\n"
    "RECC_INSTANCE - the instance name to pass to the server (defaults "
    "to \"" DEFAULT_RECC_INSTANCE "\") \n"
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
    "RECC_DEPS_EXCLUDE_PATHS - comma-separated list of paths to exclude from\n"
    "                          the input root\n"
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
    "\n"
    "RECC_RETRY_DELAY - base delay (in ms) between retries\n"
    "                   grows exponentially (default 100ms)\n"
    "\n"
    "RECC_PREFIX_MAP - specify path mappings to replace. The source and "
    "destination must both be absolute paths. \n"
    "Supports multiple paths, separated by "
    "colon(:). Ex. RECC_PREFIX_MAP=/usr/bin=/usr/local/bin)\n"
    "\n"
    "RECC_CAS_DIGEST_FUNCTION - specify what hash function to use to "
    "calculate digests.\n"
    "                           (By default, "
    "\"" DEFAULT_RECC_CAS_DIGEST_FUNCTION "\")\n"
    "                           Supported values: " +
    DigestGenerator::supportedDigestFunctionsList() +
    "\n\n"
    "RECC_WORKING_DIR_PREFIX - directory to prefix the command's working\n"
    "                          directory, and input paths relative to it\n"
    "RECC_MAX_THREADS -   Allow some operations to utilize multiple cores."
    "Default: 4 \n"
    "                     A value of -1 specifies use all available cores.\n"
    "RECC_REAPI_VERSION - Version of the Remote Execution API to use. "
    "(Default: \"" DEFAULT_RECC_REAPI_VERSION "\")\n"
    "                     Supported values: " +
    proto::reapiSupportedVersionsList());

enum ReturnCode {
    RC_OK = 0,
    RC_USAGE = 100,
    RC_EXEC_FAILURE = 101,
    RC_INVALID_GRPC_CHANNELS = 102,
    RC_INVALID_SERVER_CAPABILITIES = 103,
    RC_EXEC_ACTIONS_FAILURE = 104,
    RC_SAVING_OUTPUT_FAILURE = 105,
    RC_METRICS_PUBLISHER_INIT_FAILURE = 106
};

} // namespace

int main(int argc, char *argv[])
{
    buildboxcommon::logging::Logger::getLoggerInstance().initialize(argv[0]);

    Env::set_config_locations();
    Env::parse_config_variables();

    if (argc <= 1) {
        BUILDBOX_LOG_ERROR("USAGE: recc <command>");
        BUILDBOX_LOG_ERROR("(run \"recc --help\" for details)");
        return RC_USAGE;
    }
    else if (argc == 2 &&
             (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        BUILDBOX_LOG_WARNING(HELP);
        return RC_OK;
    }
    else if (argc == 2 && (strcmp(argv[1], "--version") == 0 ||
                           strcmp(argv[1], "-v") == 0)) {
        const std::string version =
            RequestMetadataGenerator::RECC_METADATA_TOOL_VERSION;
        const std::string versionMessage = "recc version: " + version;
        BUILDBOX_LOG_WARNING(versionMessage);
        return RC_OK;
    }

    BUILDBOX_LOG_DEBUG("RECC_REAPI_VERSION == '" << RECC_REAPI_VERSION << "'");

    std::shared_ptr<StatsDPublisherType> statsDPublisher;
    try {
        statsDPublisher = get_statsdpublisher_from_config();
    }
    catch (const std::runtime_error &e) {
        BUILDBOX_LOG_ERROR(
            "Could not initialize statsD publisher: " << e.what());
        return RC_METRICS_PUBLISHER_INIT_FAILURE;
    }

    buildboxcommon::buildboxcommonmetrics::PublisherGuard<StatsDPublisherType>
        statsDPublisherGuard(RECC_ENABLE_METRICS, *statsDPublisher);

    const std::string cwd = FileUtils::getCurrentWorkingDirectory();
    const auto command =
        ParsedCommandFactory::createParsedCommand(&argv[1], cwd.c_str());

    digest_string_umap blobs;
    digest_string_umap digest_to_filecontents;

    std::shared_ptr<proto::Action> actionPtr;
    if (command.is_compiler_command() || RECC_FORCE_REMOTE) {
        // Trying to build an `Action`:
        try {
            actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                   &digest_to_filecontents);
        }
        catch (const std::invalid_argument &) {
            BUILDBOX_LOG_ERROR(
                "Invalid `argv[0]` value in command: \"" +
                command.get_command().at(0) +
                "\". The Remote Execution API requires it to specify "
                "either a relative or absolute path to an executable.");
            return RC_EXEC_FAILURE;
        }
    }
    else {
        BUILDBOX_LOG_INFO("Not a compiler command, so running locally. (Use "
                          "RECC_FORCE_REMOTE=1 to force remote execution)");
    }

    // If we don't need to build an `Action` or if the process fails, we defer
    // to running the command locally:
    if (!actionPtr) {
        execvp(argv[1], &argv[1]);
        const std::string errorReason = strerror(errno);
        BUILDBOX_LOG_ERROR("Error executing argv[1]: " << errorReason);
        return RC_EXEC_FAILURE;
    }

    const proto::Action action = *actionPtr;
    const proto::Digest actionDigest = DigestGenerator::make_digest(action);

    BUILDBOX_LOG_DEBUG("Action Digest: " << actionDigest
                                         << " Action Contents: "
                                         << action.ShortDebugString());

    // Setting up the gRPC connections:
    std::unique_ptr<GrpcChannels> returnChannels;
    try {
        returnChannels = std::make_unique<GrpcChannels>(
            GrpcChannels::get_channels_from_config());
    }
    catch (const std::runtime_error &e) {
        BUILDBOX_LOG_ERROR("Invalid argument in channel config: " << e.what());
        return RC_INVALID_GRPC_CHANNELS;
    }

    GrpcContext grpcContext;
    grpcContext.set_action_id(actionDigest.hash());

    RemoteExecutionClient client(
        returnChannels->server(), returnChannels->cas(),
        returnChannels->action_cache(), RECC_INSTANCE, &grpcContext);

    bool action_in_cache = false;
    ActionResult result;

    // If allowed, we look in the action cache first:
    if (!RECC_SKIP_CACHE) {
        try {
            { // Timed block
                buildboxcommon::buildboxcommonmetrics::MetricGuard<
                    buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
                    mt(TIMER_NAME_QUERY_ACTION_CACHE);

                action_in_cache = client.fetch_from_action_cache(
                    actionDigest, command.get_products(), RECC_INSTANCE,
                    &result);
                if (action_in_cache) {
                    BUILDBOX_LOG_INFO("Action Cache hit for [" << actionDigest
                                                               << "]");
                }
            }
        }
        catch (const std::exception &e) {
            BUILDBOX_LOG_ERROR("Error while querying action cache at \""
                               << RECC_ACTION_CACHE_SERVER
                               << "\": " << e.what());
        }
    }

    // If the results for the action are not cached, we upload the
    // necessary resources to CAS:
    if (!action_in_cache) {
        blobs[actionDigest] = action.SerializeAsString();

        BUILDBOX_LOG_INFO("Executing action remotely... [actionDigest="
                          << actionDigest << "]");

        BUILDBOX_LOG_DEBUG("Uploading resources...");
        try {
            // We are going to make a batch request to the CAS, setting up
            // the client's max. batch size according to what the server
            // supports:
            if (RECC_CAS_GET_CAPABILITIES) {
                client.setUpFromServerCapabilities();
            }

            client.upload_resources(blobs, digest_to_filecontents);
        }
        catch (const std::exception &e) {
            BUILDBOX_LOG_ERROR("Error while uploading resources to CAS at \""
                               << RECC_CAS_SERVER << "\": " << e.what());
            return RC_INVALID_SERVER_CAPABILITIES;
        }

        // And call `Execute()`:
        try {
            // Timed block
            buildboxcommon::buildboxcommonmetrics::MetricGuard<
                buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
                mt(TIMER_NAME_EXECUTE_ACTION);

            result = client.execute_action(actionDigest, RECC_SKIP_CACHE);
            BUILDBOX_LOG_INFO("Remote execution finished with exit code "
                              << result.d_exitCode);
        }
        catch (const std::exception &e) {
            BUILDBOX_LOG_ERROR("Error while calling `Execute()` on \""
                               << RECC_SERVER << "\": " << e.what());
            return RC_EXEC_ACTIONS_FAILURE;
        }
    }

    const int exitCode = result.d_exitCode;
    try {
        /* These don't use logging macros because they are compiler output
         */
        std::cout << client.get_outputblob(result.d_stdOut);
        std::cerr << client.get_outputblob(result.d_stdErr);

        if (!RECC_DONT_SAVE_OUTPUT) {
            client.write_files_to_disk(result);
        }

        return exitCode;
    }
    catch (const std::exception &e) {
        BUILDBOX_LOG_ERROR(e.what());
        return (exitCode == 0 ? RC_SAVING_OUTPUT_FAILURE : exitCode);
    }
}
