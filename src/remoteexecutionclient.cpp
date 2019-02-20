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

#include <fileutils.h>
#include <grpcretry.h>
#include <logging.h>
#include <merklize.h>
#include <reccdefaults.h>
#include <remoteexecutionclient.h>
#include <remoteexecutionsignals.h>

#include <signal.h>

#include <functional>
#include <future>
#include <sstream>

using namespace google::longrunning;

namespace BloombergLP {
namespace recc {

std::atomic_bool RemoteExecutionClient::s_sigint_received(false);

/**
 * Return the ActionResult for the given Operation. Throws an exception
 * if the Operation finished with an error, or if the Operation hasn't
 * finished yet.
 */
proto::ActionResult get_actionresult(Operation operation)
{
    if (!operation.done()) {
        throw std::logic_error(
            "Called get_actionresult on an unfinished Operation");
    }
    else if (operation.has_error()) {
        ensure_ok(operation.error());
    }
    else if (!operation.response().Is<proto::ExecuteResponse>()) {
        throw std::runtime_error("Server returned invalid Operation result");
    }

    proto::ExecuteResponse executeResponse;
    if (!operation.response().UnpackTo(&executeResponse)) {
        throw std::runtime_error("Operation response unpacking failed");
    }

    ensure_ok(executeResponse.status());

    const proto::ActionResult actionResult = executeResponse.result();
    if (actionResult.exit_code() == 0) {
        RECC_LOG_VERBOSE("Execute response message: " +
                         executeResponse.message());
    }
    else if (!executeResponse.message().empty()) {
        RECC_LOG("Remote execution message: " + executeResponse.message());
    }

    return actionResult;
}

/**
 * Add the files from the given directory (and its subdirectories, recursively)
 * to the given outputFiles map.
 *
 * The given prefix will be appended to the file names, and the given digestMap
 * will be used to look up child directories.
 */
void add_from_directory(
    std::map<std::string, File> *outputFiles, proto::Directory directory,
    std::string prefix,
    std::unordered_map<proto::Digest, proto::Directory> digestMap)
{
    for (int i = 0; i < directory.files_size(); ++i) {
        File file;
        file.d_digest = directory.files(i).digest();
        file.d_executable = directory.files(i).is_executable();
        (*outputFiles)[prefix + directory.files(i).name()] = file;
    }

    for (int i = 0; i < directory.directories_size(); ++i) {
        auto subdirectory = digestMap[directory.directories(i).digest()];
        add_from_directory(outputFiles, subdirectory,
                           prefix + directory.directories(i).name() + "/",
                           digestMap);
    }
}

void read_operation_async(ReaderPointer reader_ptr,
                          OperationPointer operation_ptr)
{
    while (reader_ptr->Read(operation_ptr.get())) {
        if (operation_ptr->done()) {
            break;
        }
    }
}

/**
 * Read the operation into the given pointer using a separate thread so we can
 * properly handle SIGINT.
 *
 * reader_ptr->Read(&operation) is a blocking call that isn't interruptible by
 * a signal like the read() system call. It also doesn't feed its results into
 * a file descriptor, so we can't use select() without some ugly hacks. Since
 * signal handlers are very limited in power in C++, this means we need to use
 * a new thread, busy wait, and check the signal flag on each iteration.
 */
void RemoteExecutionClient::read_operation(ReaderPointer &reader_ptr,
                                           OperationPointer &operation_ptr)
{
    /* We need to block SIGINT so only this main thread catches it. */
    block_sigint();

    auto future = std::async(std::launch::async, read_operation_async,
                             reader_ptr, operation_ptr);
    unblock_sigint();

    /**
     * Wait for the operation to complete, handling the
     * cancellation flag if it was set by SIGINT.
     */
    std::future_status status;
    do {
        status = future.wait_for(DEFAULT_RECC_POLL_WAIT);
        if (RemoteExecutionClient::s_sigint_received) {
            RECC_LOG_VERBOSE("Cancelling job");
            /* Cancel the operation if the execution service gave it a name */
            if (operation_ptr->name() != "") {
                cancel_operation(operation_ptr->name());
            }
            exit(130); // Ctrl+C exit code
        }
    } while (status != std::future_status::ready);
}

ActionResult RemoteExecutionClient::execute_action(proto::Digest actionDigest,
                                                   bool skipCache)
{
    /* Prepare an asynchonous Execute request */
    proto::ExecuteRequest executeRequest;
    executeRequest.set_instance_name(d_instance);
    *executeRequest.mutable_action_digest() = actionDigest;
    executeRequest.set_skip_cache_lookup(skipCache);

    setup_signal_handler(SIGINT, RemoteExecutionClient::set_sigint_received);

    ReaderPointer reader_ptr;
    OperationPointer operation_ptr;

    /* Create the lambda to pass to grpc_retry */
    auto execute_lambda = [&](grpc::ClientContext &context) {
        reader_ptr =
            std::move(d_executionStub->Execute(&context, executeRequest));

        /* Read the result of the Execute request into an OperationPointer */
        operation_ptr = std::make_shared<Operation>();
        read_operation(reader_ptr, operation_ptr);

        return reader_ptr->Finish();
    };

    grpc_retry(execute_lambda);

    Operation operation = *operation_ptr;
    if (!operation.done()) {
        throw std::runtime_error(
            "Server closed stream before Operation finished");
    }

    auto resultProto = get_actionresult(operation);

    ActionResult result;
    result.d_exitCode = resultProto.exit_code();
    result.d_stdOut =
        OutputBlob(resultProto.stdout_raw(), resultProto.stdout_digest());
    result.d_stdErr =
        OutputBlob(resultProto.stderr_raw(), resultProto.stderr_digest());

    for (int i = 0; i < resultProto.output_files_size(); ++i) {
        auto fileProto = resultProto.output_files(i);
        File file(fileProto.digest(), fileProto.is_executable());
        result.d_outputFiles[fileProto.path()] = file;
    }

    for (int i = 0; i < resultProto.output_directories_size(); ++i) {
        auto outputDirectoryProto = resultProto.output_directories(i);
        auto tree =
            fetch_message<proto::Tree>(outputDirectoryProto.tree_digest());
        std::unordered_map<proto::Digest, proto::Directory> digestMap;
        for (int j = 0; j < tree.children_size(); ++j) {
            digestMap[make_digest(tree.children(j))] = tree.children(j);
        }
        add_from_directory(&result.d_outputFiles, tree.root(),
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

    /* Send the cancellation request and report any errors */
    google::protobuf::Empty empty;
    grpc::Status s = d_operationsStub->CancelOperation(&cancelContext,
                                                       cancelRequest, &empty);
    if (!s.ok()) {
        RECC_LOG_ERROR("Failed to cancel job " << operationName << ": "
                                               << s.error_message());
    }
    else {
        RECC_LOG("Cancelled job " << operationName);
    }
}

void RemoteExecutionClient::write_files_to_disk(ActionResult result,
                                                const char *root)
{
    for (const auto &fileIter : result.d_outputFiles) {
        std::string path = std::string(root) + "/" + fileIter.first;
        RECC_LOG_VERBOSE("Writing " << path);
        write_file(path.c_str(), fetch_blob(fileIter.second.d_digest));
        if (fileIter.second.d_executable) {
            make_executable(path.c_str());
        }
    }
}
} // namespace recc
} // namespace BloombergLP
