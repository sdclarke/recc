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

#ifndef INCLUDED_REMOTEEXECUTIONCLIENT
#define INCLUDED_REMOTEEXECUTIONCLIENT

#include <casclient.h>
#include <merklize.h>
#include <protos.h>

#include <atomic>
#include <map>

namespace BloombergLP {
namespace recc {

typedef std::shared_ptr<
    grpc::ClientReaderInterface<google::longrunning::Operation>>
    ReaderPointer;

typedef std::shared_ptr<google::longrunning::Operation> OperationPointer;

/**
 * Represents a blob returned by the Remote Execution service.
 *
 * To convert an OutputBlob to the blob it represents, use
 * RemoteExecutionClient::get_outputblob.
 */
struct OutputBlob {
    bool d_inlined;
    std::string d_blob; // Only valid if inlined.
    proto::Digest d_digest;

    OutputBlob() {}
    OutputBlob(proto::Digest digest)
        : d_inlined(digest.size_bytes() == 0), d_digest(digest)
    {
    }
    OutputBlob(std::string blob, proto::Digest digest)
        : d_blob(blob), d_digest(digest),
          d_inlined(!blob.empty() || digest.size_bytes() == 0)
    {
    }
};

struct ActionResult {
    OutputBlob d_stdOut;
    OutputBlob d_stdErr;
    int d_exitCode;
    std::map<std::string, File> d_outputFiles;
};

class RemoteExecutionClient : public CASClient {
  private:
    std::unique_ptr<proto::Execution::StubInterface> d_executionStub;
    std::unique_ptr<proto::Operations::StubInterface> d_operationsStub;
    static std::atomic_bool s_sigint_received;

    void read_operation(ReaderPointer &reader,
                        OperationPointer &operation_ptr);

    /**
     * Sends the CancelOperation RPC
     */
    void cancel_operation(const std::string &operationName);

  public:
    RemoteExecutionClient(
        proto::Execution::StubInterface *executionStub,
        proto::ContentAddressableStorage::StubInterface *casStub,
        proto::Operations::StubInterface *operationsStub,
        google::bytestream::ByteStream::StubInterface *byteStreamStub,
        std::string instance)
        : CASClient(casStub, byteStreamStub, instance),
          d_executionStub(executionStub), d_operationsStub(operationsStub)
    {
    }

    RemoteExecutionClient(std::shared_ptr<grpc::Channel> channel,
                          std::shared_ptr<grpc::Channel> casChannel,
                          std::string instance)
        : CASClient(casChannel, instance),
          d_executionStub(proto::Execution::NewStub(channel)),
          d_operationsStub(proto::Operations::NewStub(channel))
    {
    }

    RemoteExecutionClient(std::shared_ptr<grpc::Channel> channel,
                          std::string instance)
        : CASClient(channel, instance),
          d_executionStub(proto::Execution::NewStub(channel)),
          d_operationsStub(proto::Operations::NewStub(channel))
    {
    }

    RemoteExecutionClient(std::shared_ptr<grpc::Channel> channel)
        : CASClient(channel),
          d_executionStub(proto::Execution::NewStub(channel)),
          d_operationsStub(proto::Operations::NewStub(channel))
    {
    }

    /**
     * Run the action with the given digest on the given server, waiting
     * synchronously for it to complete. The Action must already be present in
     * the server's CAS.
     */
    ActionResult execute_action(proto::Digest actionDigest,
                                bool skipCache = false);

    /**
     * Get the contents of the given OutputBlob.
     */
    inline std::string get_outputblob(OutputBlob b)
    {
        return b.d_inlined ? b.d_blob : fetch_blob(b.d_digest);
    }

    /**
     * Write the given ActionResult's output files to disk.
     */
    void write_files_to_disk(ActionResult result, const char *root = ".");

    /**
     * Signal handler to mark the remote execution task for cancellation
     */
    static inline void set_sigint_received(int)
    {
        RemoteExecutionClient::s_sigint_received = true;
    }
};
} // namespace recc
} // namespace BloombergLP
#endif
