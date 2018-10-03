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

#include <deps.h>
#include <env.h>
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
    "                             the build worker\n");

void sigintHandler(int signum) {
    
}

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        cerr << "USAGE: recc <command>" << endl << endl;
        cerr << "(run \"recc --help\" for details)" << endl;
        return 1;
    }
    else if (argc == 2 &&
             (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        cerr << HELP;
        return 0;
    }

    parse_environment();

    ParsedCommand command(&argv[1]);

    if (!command.is_compiler_command() && !RECC_FORCE_REMOTE) {
        RECC_LOG_VERBOSE("Not a compiler command, so running locally.");
        RECC_LOG_VERBOSE(
            "(use RECC_FORCE_REMOTE=1 to force remote execution)");
        execvp(argv[1], &argv[1]);
        perror(argv[1]);
        exit(1);
    }

    NestedDirectory nestedDirectory;
    unordered_map<proto::Digest, string> blobs;
    unordered_map<proto::Digest, string> filenames;

    std::set<std::string> products = RECC_OUTPUT_FILES_OVERRIDE;
    if (!RECC_DEPS_DIRECTORY_OVERRIDE.empty()) {
        RECC_LOG_VERBOSE("Building Merkle tree using directory override");
        nestedDirectory = make_nesteddirectory(
            RECC_DEPS_DIRECTORY_OVERRIDE.c_str(), &filenames);
    }
    else {
        set<string> deps = RECC_DEPS_OVERRIDE;

        if (RECC_DEPS_OVERRIDE.empty() && !RECC_FORCE_REMOTE) {
            RECC_LOG_VERBOSE("Getting dependencies");
            try {
                auto fileInfo = get_file_info(command);
                deps = fileInfo.dependencies;

                if (RECC_OUTPUT_DIRECTORIES_OVERRIDE.empty() &&
                    RECC_OUTPUT_FILES_OVERRIDE.empty()) {
                    products = fileInfo.possibleProducts;
                }
            }
            catch (const subprocess_failed_error &e) {
                exit(e.error_code);
            }
        }

        for (const auto &dep : deps) {
            if (dep.compare(0, 3, "../") == 0) {
                RECC_LOG_VERBOSE("Command requires dependency outside current "
                                 "directory, so running locally.");
                RECC_LOG_VERBOSE("(use RECC_DEPS_OVERRIDE to override)");
                execvp(argv[1], &argv[1]);
                perror(argv[1]);
                exit(1);
            }
        }
        RECC_LOG_VERBOSE("Building Merkle tree");
        for (const auto &dep : deps) {
            File file(dep.c_str());
            nestedDirectory.add(file, dep.c_str());
            filenames[file.digest] = dep;
        }
    }
    for (const auto &product : products) {
        if (product.compare(0, 3, "../") == 0 || product[0] == '/') {
            RECC_LOG_VERBOSE("Command produces file outside current "
                             "directory, so running locally.");
            RECC_LOG_VERBOSE(
                "(use RECC_OUTPUT_[FILES|DIRECTORIES]_OVERRIDE to override)");
            execvp(argv[1], &argv[1]);
            perror(argv[1]);
            exit(1);
        }
    }

    auto directoryDigest = nestedDirectory.to_digest(&blobs);

    proto::Command commandProto;

    for (const auto &arg : command.get_command()) {
        commandProto.add_arguments(arg);
    }
    for (const auto &envIter : RECC_REMOTE_ENV) {
        auto envVar = commandProto.add_environment_variables();
        envVar->set_name(envIter.first);
        envVar->set_value(envIter.second);
    }
    for (const auto &product : products) {
        commandProto.add_output_files(product);
    }
    for (const auto &directory : RECC_OUTPUT_DIRECTORIES_OVERRIDE) {
        commandProto.add_output_directories(directory);
    }
    for (const auto &platformIter : RECC_REMOTE_PLATFORM) {
        auto property = commandProto.mutable_platform()->add_properties();
        property->set_name(platformIter.first);
        property->set_value(platformIter.second);
    }
    RECC_LOG_VERBOSE("Command: " << commandProto.ShortDebugString());
    auto commandDigest = make_digest(commandProto);
    blobs[commandDigest] = commandProto.SerializeAsString();

    proto::Action action;
    *action.mutable_command_digest() = commandDigest;
    *action.mutable_input_root_digest() = directoryDigest;
    action.set_do_not_cache(RECC_ACTION_UNCACHEABLE);
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

    cout << client.get_outputblob(result.stdOut);
    cerr << client.get_outputblob(result.stdErr);

    if (!RECC_DONT_SAVE_OUTPUT) {
        client.write_files_to_disk(result);
    }

    return result.exitCode;
}
