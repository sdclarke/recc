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

#include <remoteexecutionclient.h>

#include <fileutils.h>
#include <logging.h>
#include <merklize.h>
#include <signal.h>
#include <unistd.h>

using namespace std;

namespace BloombergLP {
namespace recc {

volatile sig_atomic_t RemoteExecutionClient::cancelled = 0;

/**
 * Return the ActionResult for the given Operation. Throws an exception
 * if the Operation finished with an error, or if the Operation hasn't
 * finished yet.
 */
proto::ActionResult get_actionresult(google::longrunning::Operation operation)
{
    if (!operation.done()) {
        throw logic_error(
            "Called get_actionresult on an unfinished Operation");
    }
    else if (operation.has_error()) {
        ensure_ok(operation.error());
    }
    else if (!operation.response().Is<proto::ExecuteResponse>()) {
        throw runtime_error("Server returned invalid Operation result");
    }
    proto::ExecuteResponse executeResponse;
    if (!operation.response().UnpackTo(&executeResponse)) {
        throw runtime_error("Operation response unpacking failed");
    }
    ensure_ok(executeResponse.status());
    return executeResponse.result();
}

/**
 * Add the files from the given directory (and its subdirectories, recursively)
 * to the given outputFiles map.
 *
 * The given prefix will be appended to the file names, and the given digestMap
 * will be used to look up child directories.
 */
void add_from_directory(
    map<string, File> *outputFiles, proto::Directory directory, string prefix,
    unordered_map<proto::Digest, proto::Directory> digestMap)
{
    for (int i = 0; i < directory.files_size(); ++i) {
        File file;
        file.digest = directory.files(i).digest();
        file.executable = directory.files(i).is_executable();
        (*outputFiles)[prefix + directory.files(i).name()] = file;
    }

    for (int i = 0; i < directory.directories_size(); ++i) {
        auto subdirectory = digestMap[directory.directories(i).digest()];
        add_from_directory(outputFiles, subdirectory,
                           prefix + directory.directories(i).name() + "/",
                           digestMap);
    }
}

ActionResult RemoteExecutionClient::execute_action(proto::Digest actionDigest,
                                                   bool skipCache)
{
    proto::ExecuteRequest executeRequest;
    executeRequest.set_instance_name(instance);
    *executeRequest.mutable_action_digest() = actionDigest;
    executeRequest.set_skip_cache_lookup(skipCache);

    grpc::ClientContext context;

    /* Set up signal handling for the Execute() request */
    struct sigaction sa;
    sa.sa_handler = RemoteExecutionClient::set_cancelled_flag;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        cerr << "Warning: unable to register cancellation handler" << endl;
    }
    auto reader = stub->Execute(&context, executeRequest);

    google::longrunning::Operation operation;
    while (reader->Read(&operation)) {
        if (RemoteExecutionClient::cancelled) {
            RECC_LOG_VERBOSE("Cancelling job");
            cancel_operation(operation.name());

            /* Create and return a dummy ActionResult to denote cancellation */
            ActionResult result;
            result.exitCode = 130; // Ctrl+C exit code
            return result;
        }
        if (operation.done()) {
            break;
        }
    }
    ensure_ok(reader->Finish());

    if (!operation.done()) {
        throw runtime_error("Server closed stream before Operation finished");
    }
    auto resultProto = get_actionresult(operation);

    ActionResult result;
    result.exitCode = resultProto.exit_code();
    result.stdOut =
        OutputBlob(resultProto.stdout_raw(), resultProto.stdout_digest());
    result.stdErr =
        OutputBlob(resultProto.stderr_raw(), resultProto.stderr_digest());

    for (int i = 0; i < resultProto.output_files_size(); ++i) {
        auto fileProto = resultProto.output_files(i);
        File file(fileProto.digest(), fileProto.is_executable());
        result.outputFiles[fileProto.path()] = file;
    }

    for (int i = 0; i < resultProto.output_directories_size(); ++i) {
        auto outputDirectoryProto = resultProto.output_directories(i);
        auto tree =
            fetch_message<proto::Tree>(outputDirectoryProto.tree_digest());
        unordered_map<proto::Digest, proto::Directory> digestMap;
        for (int j = 0; j < tree.children_size(); ++j) {
            digestMap[make_digest(tree.children(j))] = tree.children(j);
        }
        add_from_directory(&result.outputFiles, tree.root(),
                           outputDirectoryProto.path() + "/", digestMap);
    }

    return result;
}

void RemoteExecutionClient::cancel_operation(const std::string &operationName)
{
    proto::CancelOperationRequest cancelRequest;
    cancelRequest.set_name(operationName);

    /* Can't use the same context for simultaneous async RPCs */
    grpc::ClientContext cancelContext;

    /* Send the execution request and report any errors */
    grpc::Status s = operationsStub->CancelOperation(&cancelContext,
                                                     cancelRequest, nullptr);
    if (!s.ok()) {
        cerr << "Failed to cancel job " << operationName << ": "
             << s.error_message() << endl;
    }
    else {
        cout << "Cancelled job " << operationName << endl;
    }
}

void RemoteExecutionClient::write_files_to_disk(ActionResult result,
                                                const char *root)
{
    for (const auto &fileIter : result.outputFiles) {
        string path = string(root) + "/" + fileIter.first;
        RECC_LOG_VERBOSE("Writing " << path);
        write_file(path.c_str(), fetch_blob(fileIter.second.digest));
        if (fileIter.second.executable) {
            make_executable(path.c_str());
        }
    }
}
} // namespace recc
} // namespace BloombergLP
