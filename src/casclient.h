// Copyright 2019 Bloomberg Finance L.P
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

#ifndef INCLUDED_CASCLIENT
#define INCLUDED_CASCLIENT

#include <grpccontext.h>
#include <grpcpp/channel.h>
#include <merklize.h>
#include <protos.h>

#include <google/bytestream/bytestream.grpc.pb.h>

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace BloombergLP {
namespace recc {

typedef std::shared_ptr<grpc::Channel> channel_ref;

class PreconditionFail : public std::exception {
  private:
    const std::vector<std::string> d_missingFiles;

  public:
    PreconditionFail(const std::vector<std::string> &missingFileList)
        : d_missingFiles(missingFileList)
    {
    }
    const char *what() const noexcept
    {
        return "Precondition Failed: Blobs Not Found";
    }
    const std::vector<std::string> &get_missing_files() const
    {
        return this->d_missingFiles;
    }
};

class CASClient {
  private:
    std::shared_ptr<proto::ContentAddressableStorage::StubInterface>
        d_executionStub;
    std::shared_ptr<google::bytestream::ByteStream::StubInterface>
        d_byteStreamStub;

    static const int s_byteStreamChunkSizeBytes;
    static const int s_maxTotalBatchSizeBytes;
    static const int s_maxMissingBlobsRequestItems;

    static const std::string s_guid;

  protected:
    // Accessed by child class `RemoteExecutionClient`
    const std::string d_instanceName;
    GrpcContext *d_grpcContext;

  public:
    explicit CASClient(
        std::shared_ptr<proto::ContentAddressableStorage::StubInterface>
            executionStub,
        std::shared_ptr<google::bytestream::ByteStream::StubInterface>
            byteStreamStub,
        const std::string &instanceName, GrpcContext *grpcContext)
        : d_executionStub(executionStub), d_byteStreamStub(byteStreamStub),
          d_instanceName(instanceName), d_grpcContext(grpcContext)
    {
    }

    explicit CASClient(std::shared_ptr<grpc::Channel> channel,
                       const std::string &instanceName,
                       GrpcContext *grpcContext)
        : d_executionStub(proto::ContentAddressableStorage::NewStub(channel)),
          d_byteStreamStub(google::bytestream::ByteStream::NewStub(channel)),
          d_instanceName(instanceName), d_grpcContext(grpcContext)
    {
    }

    /**
     * Unconditionally upload a blob using the ByteStream API.
     */
    void upload_blob(const proto::Digest &digest,
                     const std::string &blob) const;

    /**
     * Fetch a blob using the ByteStream API.
     */
    std::string fetch_blob(const proto::Digest &digest) const;

    /**
     * Fetch a message using the ByteStream API.
     */
    template <typename Msg>
    inline Msg fetch_message(const proto::Digest &digest)
    {
        Msg result;
        if (!result.ParseFromString(fetch_blob(digest))) {
            throw std::runtime_error("Could not deserialize fetched message");
        }
        return result;
    }

    /**
     * Upload the given resources to the CAS server. This first sends a
     * FindMissingBlobsRequest to determine which resources need to be
     * uploaded, then uses the ByteStream and BatchUpdateBlobs APIs to upload
     * them.
     */
    void
    upload_resources(const digest_string_umap &blobs,
                     const digest_string_umap &digest_to_filecontents) const;

  private:
    std::string uploadResourceName(const proto::Digest &digest) const;
    std::string downloadResourceName(const proto::Digest &digest) const;

    std::unordered_set<proto::Digest>
    findMissingBlobs(const std::unordered_set<proto::Digest> &digests) const;

    proto::FindMissingBlobsResponse
    findMissingBlobs(const proto::FindMissingBlobsRequest &request) const;

    void
    batchUpdateBlobs(const std::unordered_set<proto::Digest> &digests,
                     const digest_string_umap &blobs,
                     const digest_string_umap &digest_to_filecontents) const;

    proto::BatchUpdateBlobsResponse
    batchUpdateBlobs(const proto::BatchUpdateBlobsRequest &request) const;

    static std::string generate_guid();
};
} // namespace recc
} // namespace BloombergLP

#endif
