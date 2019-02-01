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

// bin/reccworker.cpp
//
// Runs a build worker.

#include <casclient.h>
#include <env.h>
#include <fileutils.h>
#include <grpcchannels.h>
#include <logging.h>
#include <merklize.h>
#include <protos.h>
#include <recccounterguard.h>
#include <reccdefaults.h>
#include <subprocess.h>

#include <condition_variable>
#include <cstring>
#include <dirent.h>
#include <exception>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <thread>
#include <unistd.h>

using namespace BloombergLP::recc;

const unsigned long MAX_PRINT = 25;
const std::string HELP(
    "USAGE: reccworker [id]\n"
    "\n"
    "Start a remote worker with the given ID. If the ID is unspecified,\n"
    "the computer's hostname and reccworker's process ID will be combined\n"
    "to make the worker's ID.\n"
    "\n"
    "The following environment variables can be used to change reccworker's\n"
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
    "RECC_MAX_CONCURRENT_JOBS - number of jobs to run simultaneously\n"
    "\n"
    "RECC_JOBS_COUNT - number of jobs to run before terminating the worker\n"
    "                  (by default, unlimited)"
    "\n"
    "RECC_INSTANCE - the instance name to pass to the server\n"
    "\n"
    "RECC_VERBOSE - enable verbose output\n"
    "\n"
    "TMPDIR - directory used to store temporary files\n"
    "\n"
    "RECC_REMOTE_PLATFORM_[key] - specifies this worker's platform\n"
    "                             properties");

/**
 * Execute the given Action synchronously, using the given CASClient to fetch
 * the necessary resources.
 */
proto::ActionResult execute_action(proto::Action action, CASClient &casClient)
{
    TemporaryDirectory tmpdir(DEFAULT_RECC_WORKER_TMP_PREFIX);
    RECC_LOG_VERBOSE("Starting build in temporary directory "
                     << tmpdir.name());

    std::vector<std::string> missing;
    casClient.download_directory(action.input_root_digest(), tmpdir.name(),
                                 &missing);

    // if missing is not empty, that means that some expected
    // files are missing. We should throw a failed precondition
    if (!missing.empty()) {
        throw PreconditionFail(missing);
    }

    const std::string pathPrefix = std::string(tmpdir.name()) + "/";

    // Fetch command from CAS and read it
    const auto commandProto =
        casClient.fetch_message<proto::Command>(action.command_digest());
    std::vector<std::string> command;
    for (auto &arg : commandProto.arguments()) {
        command.push_back(arg);
    }
    std::map<std::string, std::string> env;
    for (auto &envVar : commandProto.environment_variables()) {
        env[envVar.name()] = envVar.value();
    }
    const std::string workingDirectory =
        pathPrefix + commandProto.working_directory();
    const std::string outputPathPrefix = workingDirectory + "/";

    // Create parent directories for the Command's output files
    for (auto &file : commandProto.output_files()) {
        if (file.find("/") != std::string::npos) {
            create_directory_recursive(
                (outputPathPrefix + file.substr(0, file.rfind("/"))).c_str());
        }
    }
    for (auto &directory : commandProto.output_directories()) {
        create_directory_recursive((pathPrefix + directory).c_str());
    }

    RECC_LOG_VERBOSE("Changing to " << workingDirectory
                                    << " and running command "
                                    << commandProto.DebugString());
    const auto subprocessResult =
        execute(command, true, true, env, workingDirectory.c_str());
    RECC_LOG_VERBOSE("Command completed. Creating action result...");

    proto::ActionResult result;
    result.set_exit_code(subprocessResult.d_exitCode);
    result.set_stdout_raw(subprocessResult.d_stdOut);
    result.set_stderr_raw(subprocessResult.d_stdErr);

    std::unordered_map<proto::Digest, std::string> blobsToUpload;
    std::unordered_map<proto::Digest, std::string> filesToUpload;

    // Add any output files that exist to ActionResult and filesToUpload
    for (auto &outputFilename : commandProto.output_files()) {
        const auto outputPath = outputPathPrefix + outputFilename;
        if (access(outputPath.c_str(), R_OK) == 0) {
            RECC_LOG_VERBOSE("Making digest for " << outputPath);
            const auto file = File(outputPath.c_str());
            filesToUpload[file.d_digest] = outputPath;
            auto fileProto = result.add_output_files();
            fileProto->set_path(outputFilename);
            fileProto->set_is_executable(file.d_executable);
            *fileProto->mutable_digest() = file.d_digest;
        }
    }

    // Construct output directory trees, add them to ActionResult and
    // blobsToUpload
    for (auto &directory : commandProto.output_directories()) {
        const auto directoryPath = outputPathPrefix + directory;
        if (access(directoryPath.c_str(), R_OK) == 0) {
            const auto tree =
                make_nesteddirectory(directoryPath.c_str(), &filesToUpload)
                    .to_tree();
            const auto treeDigest = make_digest(tree);
            blobsToUpload[treeDigest] = tree.SerializeAsString();

            auto outputDirectory = result.add_output_directories();
            outputDirectory->set_path(directory);
            *outputDirectory->mutable_tree_digest() = treeDigest;
        }
    }

    RECC_LOG_VERBOSE("Uploading resources...");

    casClient.upload_resources(blobsToUpload, filesToUpload);

    RECC_LOG_VERBOSE("ActionResult " << result.DebugString());

    return result;
}

/**
 * Executes the build job corresponding to the given lease ID in the given
 * session, updates the session with the results, and removes the job from the
 * activeJobs set.
 *
 * The given session mutex will be used to synchronize access to the session
 * and activeJobs set. `sessionCondition` will be notified once the job has
 * completed. `casChannel` will be used to connect to the CAS service.
 */
void worker_thread(proto::BotSession *session,
                   std::set<std::string> *activeJobs, std::mutex *sessionMutex,
                   std::condition_variable *sessionCondition,
                   std::shared_ptr<grpc::Channel> casChannel,
                   std::string leaseID)
{
    proto::ActionResult result;
    bool preconditionFail = false;
    std::ostringstream missingFiles;

    try {
        CASClient casClient(casChannel, RECC_INSTANCE);
        proto::Action action;
        {
            // Get the Action to execute from the session.
            std::lock_guard<std::mutex> lock(*sessionMutex);
            bool foundLease = false;
            for (auto &lease : session->leases()) {
                if (lease.id() == leaseID) {
                    if (lease.payload().Is<proto::Action>()) {
                        lease.payload().UnpackTo(&action);
                    }
                    else if (lease.payload().Is<proto::Digest>()) {
                        proto::Digest actionDigest;
                        lease.payload().UnpackTo(&actionDigest);
                        action = casClient.fetch_message<proto::Action>(
                            actionDigest);
                    }
                    else {
                        throw std::runtime_error("Invalid lease payload type");
                    }
                    foundLease = true;
                    break;
                }
            }
            if (!foundLease) {
                return;
            }
        }

        result = execute_action(action, casClient);
    }
    catch (const PreconditionFail &e) {
        preconditionFail = true;
        std::ostringstream errorMessage;
        errorMessage
            << "Precondition Failed, unable to find requested blobs in CAS";
        RECC_LOG_ERROR(errorMessage.str());
        const std::vector<std::string> &missingFilesVec =
            e.get_missing_files();
        for (int i = 0; i < missingFilesVec.size(); i++) {
            missingFiles << missingFilesVec[i] << ", ";
            if (i == std::min(MAX_PRINT, missingFilesVec.size() - 1)) {
                RECC_LOG_VERBOSE("Missing Blobs: " << missingFiles.str());
            }
        }
        result.set_stderr_raw(errorMessage.str() + "\n");
        result.set_exit_code(1);
    }
    catch (const std::exception &e) {
        result.set_exit_code(1);
        RECC_LOG_VERBOSE("Caught exception in worker: " << e.what());
        result.set_stderr_raw(std::string("Exception in worker: ") + e.what() +
                              std::string("\n"));
    }

    {
        // Store the ActionResult back in the lease.
        //
        // We need to search for the lease again because it could have been
        // moved/cancelled/deleted/completed by someone else while we were
        // executing it.
        std::lock_guard<std::mutex> lock(*sessionMutex);
        for (auto &lease : *session->mutable_leases()) {
            if (lease.id() == leaseID &&
                lease.state() == proto::LeaseState::ACTIVE) {
                lease.set_state(proto::LeaseState::COMPLETED);

                if (preconditionFail) {
                    auto status = lease.mutable_status();
                    status->set_message(missingFiles.str());
                    status->set_code(grpc::FAILED_PRECONDITION);
                }

                lease.mutable_result()->PackFrom(result);
            }
        }
        activeJobs->erase(leaseID);
    }
    sessionCondition->notify_all();
}

std::string generate_default_bot_id()
{
    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname));
    return std::string(hostname) + "-" + std::to_string(getpid());
}

int main(int argc, char *argv[])
{
    std::string bot_id = generate_default_bot_id();

    // Parse command-line arguments.
    if (argc > 2) {
        RECC_LOG_ERROR("USAGE: " << argv[0] << " [id]");
        RECC_LOG_ERROR("(run \"" << argv[0] << " --help\" for details)");
        return 1;
    }
    else if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            RECC_LOG_WARNING(HELP);
            return 0;
        }
        bot_id = std::string(argv[1]);
    }

    // Parse configuration from environment variables and defaults
    // Specify Reccworker in argument to set reccworker specific variables
    set_config_locations();
    parse_config_variables(Source::e_Reccworker);

    ReccCounterGuard counterGuard(
        ReccCounterGuard::get_limit_from_args(RECC_JOBS_COUNT));

    // If RECC_JOBS_LIMIT was set, make sure we cap
    // RECC_MAX_CONCURRENT_JOBS
    if (!counterGuard.is_unlimited()) {
        if (RECC_MAX_CONCURRENT_JOBS > counterGuard.get_limit()) {
            RECC_LOG_WARNING(
                "Warning: RECC_MAX_CONCURRENT_JOBS ("
                << RECC_MAX_CONCURRENT_JOBS << ") > RECC_JOBS_COUNT ("
                << counterGuard.get_limit() << ") "
                << ", capping it to: " << counterGuard.get_limit());
            RECC_MAX_CONCURRENT_JOBS = counterGuard.get_limit();
        }
    }

    RECC_LOG_VERBOSE("Starting build worker with bot_id=[" + bot_id + "]");
    GrpcChannels returnChannels = GrpcChannels::get_channels_from_config();
    auto stub = proto::Bots::NewStub(returnChannels.server());

    proto::BotSession session;
    session.set_bot_id(bot_id);
    session.set_status(proto::BotStatus::OK);

    auto worker = session.mutable_worker();
    auto device = worker->add_devices();
    device->set_handle(DEFAULT_RECC_INSTANCE);

    for (auto &platformPair : RECC_REMOTE_PLATFORM) {
        // TODO Differentiate worker properties and device properties?
        auto workerProperty = worker->add_properties();
        workerProperty->set_key(platformPair.first);
        workerProperty->set_key(platformPair.second);

        auto deviceProperty = device->add_properties();
        deviceProperty->set_key(platformPair.first);
        deviceProperty->set_key(platformPair.second);
    }

    {
        // Send the initial request to create the bot session.
        grpc::ClientContext context;
        proto::CreateBotSessionRequest createRequest;
        RECC_LOG_VERBOSE("Setting parent");
        createRequest.set_parent(RECC_INSTANCE);
        *createRequest.mutable_bot_session() = session;
        RECC_LOG_VERBOSE("Setting session");
        ensure_ok(stub->CreateBotSession(&context, createRequest, &session));
    }

    RECC_LOG_VERBOSE("Bot Session created. Now waiting for jobs.");

    std::mutex sessionMutex;
    std::condition_variable sessionCondition;
    bool skipPollDelay = true;

    // Keep track of Lease IDs that have been accepted but not assigned to a
    // worker yet. Fully managed by the while-loop; used to know if it is
    // permitted to exit upon reaching the JOBS_LIMIT
    std::set<std::string> activeJobsPendingWorker;

    // Keep track of Lease IDs that have been assigned to a worker
    // Lease IDs are added when the while-loop spawns worker threads and they
    //           are removed when by the worker threads when they're done
    std::set<std::string> activeJobs;

    // Run this loop until the job limit (if any) was reached, or there
    // are jobs running, or jobs have been accepted and pending a
    // worker assignment
    while (counterGuard.is_allowed_more() || activeJobs.size() ||
           activeJobsPendingWorker.size()) {
        std::unique_lock<std::mutex> lock(sessionMutex);
        if (skipPollDelay) {
            skipPollDelay = false;
        }
        else {
            // Wait for a build job to finish or 250 ms (whichever comes first)
            sessionCondition.wait_for(lock, DEFAULT_RECC_WORKER_POLL_WAIT);
        }
        for (auto &lease : *session.mutable_leases()) {
            if (lease.state() == proto::LeaseState::PENDING) {
                RECC_LOG_VERBOSE("Got lease: " << lease.DebugString());
                if (lease.payload().Is<proto::Action>() ||
                    lease.payload().Is<proto::Digest>()) {
                    if (activeJobs.size() + activeJobsPendingWorker.size() <
                            RECC_MAX_CONCURRENT_JOBS &&
                        counterGuard.is_allowed_more()) {
                        // Accept the lease, but wait for the server's ack
                        // before actually starting work on it.
                        lease.set_state(proto::LeaseState::ACTIVE);
                        skipPollDelay = true;
                        activeJobsPendingWorker.insert(lease.id());
                        counterGuard.decrease_limit();
                    }
                }
                else {
                    lease.set_state(proto::LeaseState::COMPLETED);
                    auto status = lease.mutable_status();
                    status->set_message("Invalid lease");
                    status->set_code(google::rpc::Code::INVALID_ARGUMENT);
                }
            }
            else if (lease.state() == proto::LeaseState::ACTIVE) {
                if (activeJobs.count(lease.id()) == 0) {
                    auto thread = std::thread(
                        worker_thread, &session, &activeJobs, &sessionMutex,
                        &sessionCondition, returnChannels.cas(), lease.id());
                    thread.detach();
                    // (the thread will remove its job from activeJobs when
                    // it's done)
                    activeJobs.insert(lease.id());

                    // since a worker thread has been spawned, remove it from
                    // the pending set
                    activeJobsPendingWorker.erase(lease.id());
                }
            }
        }
        {
            // Update the bot session (send our state to the server, then
            // replace our state with the server's response)
            grpc::ClientContext context;
            proto::UpdateBotSessionRequest updateRequest;
            updateRequest.set_name(session.name());
            *updateRequest.mutable_bot_session() = session;
            ensure_ok(
                stub->UpdateBotSession(&context, updateRequest, &session));
        }
    }
    if (!counterGuard.is_allowed_more()) {
        RECC_LOG_VERBOSE("Terminating, reached job limit.");
    }
    return 0;
}
