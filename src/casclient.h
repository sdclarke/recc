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

#ifndef INCLUDED_CASCLIENT
#define INCLUDED_CASCLIENT

#include <protos.h>

#include <google/bytestream/bytestream.grpc.pb.h>
#include <grpcpp/channel.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace BloombergLP {
namespace recc {

typedef std::shared_ptr<grpc::Channel> channel_ref;

class CASClient {
  private:
    std::unique_ptr<proto::ContentAddressableStorage::StubInterface>
        executionStub;
    std::unique_ptr<google::bytestream::ByteStream::StubInterface>
        byteStreamStub;

  public:
    std::string instance;

    CASClient(proto::ContentAddressableStorage::StubInterface *executionStub,
              google::bytestream::ByteStream::StubInterface *byteStreamStub,
              std::string instance)
        : executionStub(executionStub), byteStreamStub(byteStreamStub),
          instance(instance)
    {
    }

    CASClient(std::shared_ptr<grpc::Channel> channel, std::string instance)
        : executionStub(proto::ContentAddressableStorage::NewStub(channel)),
          byteStreamStub(google::bytestream::ByteStream::NewStub(channel)),
          instance(instance)
    {
    }

    CASClient(std::shared_ptr<grpc::Channel> channel)
        : executionStub(proto::ContentAddressableStorage::NewStub(channel)),
          byteStreamStub(google::bytestream::ByteStream::NewStub(channel)),
          instance()
    {
    }

    /**
     * Unconditionally upload a blob using the ByteStream API.
     */
    void upload_blob(proto::Digest digest, std::string blob);

    /**
     * Fetch a blob using the ByteStream API.
     */
    std::string fetch_blob(proto::Digest digest);

    /**
     * Fetch a message using the ByteStream API.
     */
    template <typename Msg> inline Msg fetch_message(proto::Digest digest)
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
    upload_resources(std::unordered_map<proto::Digest, std::string> blobs,
                     std::unordered_map<proto::Digest, std::string> filenames);

    /**
     * Download the directory with the given digest, storing its contents at
     * the given path.
     *
     * The digest must correspond to a Directory message, and the path must be
     * a directory that already exists.
     */
    void download_directory(proto::Digest digest, const char *path);
};
} // namespace recc
} // namespace BloombergLP

#endif
