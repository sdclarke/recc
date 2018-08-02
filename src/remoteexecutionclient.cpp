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

using namespace std;

namespace BloombergLP {
namespace recc {

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

proto::ActionResult
RemoteExecutionClient::wait_operation(google::longrunning::Operation operation)
{
    if (operation.done()) {
        return get_actionresult(operation);
    }
    google::watcher::v1::Request watcherRequest;
    watcherRequest.set_target(operation.name());

    grpc::ClientContext context;
    auto reader = watcherStub->Watch(&context, watcherRequest);
    google::watcher::v1::ChangeBatch changeBatch;
    while (reader->Read(&changeBatch)) {
        RECC_LOG_VERBOSE(
            "Got changebatch: " << changeBatch.ShortDebugString());
        for (int i = 0; i < changeBatch.changes_size(); ++i) {
            auto change = changeBatch.changes(i);
            if (change.data().Is<google::rpc::Status>()) {
                google::rpc::Status status;
                if (change.data().UnpackTo(&status)) {
                    ensure_ok(status);
                }
                else {
                    throw runtime_error("ChangeBatch error unpacking failed");
                }
            }
            else if (change.data().Is<google::longrunning::Operation>()) {
                if (!change.data().UnpackTo(&operation)) {
                    throw runtime_error(
                        "ChangeBatch Operation unpacking failed");
                }
                if (operation.done()) {
                    ensure_ok(reader->Finish());
                    return get_actionresult(operation);
                }
            }
            else {
                throw runtime_error("Invalid ChangeBatch data: " +
                                    change.data().ShortDebugString());
            }
        }
    }
    ensure_ok(reader->Finish());
    throw runtime_error(
        "Reader message stream ended without Operation completing");
}

/**
 * Add the files from the given directory (and its subdirectories, recursively)
 * to the given outputFiles map.
 *
 * The given prefix will be appended to the file names, and the given digestMap
 * will be used to look up child directories.
 */
void add_from_directory(
    map<string, OutputFile> *outputFiles, proto::Directory directory,
    string prefix, unordered_map<proto::Digest, proto::Directory> digestMap)
{
    for (int i = 0; i < directory.files_size(); ++i) {
        OutputFile file;
        file.executable = directory.files(i).is_executable();
        file.content = OutputBlob(directory.files(i).digest());
        (*outputFiles)[prefix + directory.files(i).name()] = file;
    }

    for (int i = 0; i < directory.directories_size(); ++i) {
        auto subdirectory = digestMap[directory.directories(i).digest()];
        add_from_directory(outputFiles, subdirectory,
                           prefix + directory.directories(i).name() + "/",
                           digestMap);
    }
}

ActionResult RemoteExecutionClient::execute_action(proto::Action action,
                                                   bool skipCache)
{
    proto::ExecuteRequest executeRequest;
    executeRequest.set_instance_name(instance);
    *executeRequest.mutable_action() = action;
    executeRequest.set_skip_cache_lookup(skipCache);

    grpc::ClientContext context;
    google::longrunning::Operation operation;
    ensure_ok(stub->Execute(&context, executeRequest, &operation));

    auto resultProto = wait_operation(operation);

    ActionResult result;
    result.exitCode = resultProto.exit_code();
    result.stdOut =
        OutputBlob(resultProto.stdout_raw(), resultProto.stdout_digest());
    result.stdErr =
        OutputBlob(resultProto.stderr_raw(), resultProto.stderr_digest());

    for (int i = 0; i < resultProto.output_files_size(); ++i) {
        auto fileProto = resultProto.output_files(i);
        OutputFile file;
        file.executable = fileProto.is_executable();
        file.content = OutputBlob(fileProto.content(), fileProto.digest());
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

void RemoteExecutionClient::write_files_to_disk(ActionResult result,
                                                const char *root)
{
    for (const auto &fileIter : result.outputFiles) {
        string path = string(root) + "/" + fileIter.first;
        RECC_LOG_VERBOSE("Writing " << path);
        auto blob = get_outputblob(fileIter.second.content);
        write_file(path.c_str(), get_outputblob(fileIter.second.content));
        if (fileIter.second.executable) {
            make_executable(path.c_str());
        }
    }
}
} // namespace recc
} // namespace BloombergLP
