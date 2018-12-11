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
#include <env.h>
#include <fileutils.h>
#include <logging.h>
#include <merklize.h>
#include <reccdefaults.h>
#include <remoteexecutionclient.h>

#include <cstdio>
#include <cstring>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using namespace BloombergLP::recc;
using namespace std;

const string HELP(
    "USAGE: recc <command>\n"
    "\n"
    "If the given command is a compile command, runs it on a remote build\n"
    "server. Otherwise, runs it locally.\n"
    "\n"
    "The following environment variables can be used to change recc's\n"
    "behavior:\n"
    "\n"
    "RECC_SERVER - the URI of the server to use (e.g. localhost:8085)\n"
    "\n"
    "RECC_CAS_SERVER - the URI of the CAS server to use (by default, we\n"
    "                  use RECC_SERVER)\n"
    "\n"
    "RECC_SERVER_AUTH_GOOGLEAPI - use default google authentication when\n"
    "                             communicating over gRPC, instead of\n"
    "                             using an insecure connection\n"
    "\n"
    "RECC_INSTANCE - the instance name to pass to the server\n"
    "\n"
    "RECC_VERBOSE - enable verbose output\n"
    "\n"
    "RECC_FORCE_REMOTE - send the command to the build server even if it\n"
    "                    isn't a compile command\n"
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
    "RECC_DEPS_OVERRIDE - comma-separated list of files to send to the\n"
    "                     build server (by default, we run `deps` to\n"
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
    "                             the build worker");

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

    set_config_locations();
    parse_config_variables();
    const string cwd = get_current_working_directory();
    ParsedCommand command(&argv[1], cwd.c_str());

    unordered_map<proto::Digest, string> blobs;
    unordered_map<proto::Digest, string> filenames;

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
    RECC_LOG_VERBOSE("Action: " << action.ShortDebugString());
    auto actionDigest = make_digest(action);

    blobs[actionDigest] = action.SerializeAsString();
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (RECC_SERVER_AUTH_GOOGLEAPI) {
        creds = grpc::GoogleDefaultCredentials();
    }
    else {
        creds = grpc::InsecureChannelCredentials();
    }
    auto channel = grpc::CreateChannel(RECC_SERVER, creds);
    auto casChannel = grpc::CreateChannel(RECC_CAS_SERVER, creds);
    RemoteExecutionClient client(channel, casChannel, RECC_INSTANCE);
    RECC_LOG_VERBOSE("Uploading resources...");
    client.upload_resources(blobs, filenames);
    RECC_LOG_VERBOSE("Executing action...");
    auto result = client.execute_action(actionDigest, RECC_SKIP_CACHE);

    /* These don't use logging macros because they are compiler output */
    cout << client.get_outputblob(result.d_stdOut);
    cerr << client.get_outputblob(result.d_stdErr);

    if (!RECC_DONT_SAVE_OUTPUT) {
        client.write_files_to_disk(result);
    }

    return result.d_exitCode;
}
