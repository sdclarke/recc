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

#ifndef INCLUDED_PROTOS
#define INCLUDED_PROTOS

#include <build/bazel/remote/execution/v2/remote_execution.grpc.pb.h>
#include <build/bazel/remote/execution/v2/remote_execution.pb.h>
#include <google/bytestream/bytestream.grpc.pb.h>
#include <google/bytestream/bytestream.pb.h>
#include <google/devtools/remoteworkers/v1test2/bots.grpc.pb.h>
#include <google/devtools/remoteworkers/v1test2/bots.pb.h>
#include <google/devtools/remoteworkers/v1test2/worker.grpc.pb.h>
#include <google/devtools/remoteworkers/v1test2/worker.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <google/longrunning/operations.pb.h>
#include <google/rpc/code.pb.h>
#include <google/rpc/status.pb.h>

namespace BloombergLP {
namespace recc {
namespace proto {
using namespace build::bazel::remote::execution::v2;
using namespace google::longrunning;
using namespace google::devtools::remoteworkers::v1test2;
} // namespace proto

/**
 * If the given Status isn't OK, throw an exception.
 */
inline void ensure_ok(const grpc::Status &status)
{
    if (!status.ok()) {
        throw std::runtime_error("GRPC error " +
                                 std::to_string(status.error_code()) + ": " +
                                 status.error_message());
    }
}
inline void ensure_ok(const google::rpc::Status &status)
{
    if (status.code() != google::rpc::Code::OK) {
        throw std::runtime_error(status.ShortDebugString());
    }
}

} // namespace recc
} // namespace BloombergLP

// Allow Digests to be used as unordered_hash keys.
namespace std {
template <> struct hash<BloombergLP::recc::proto::Digest> {
    std::size_t
    operator()(const BloombergLP::recc::proto::Digest &digest) const noexcept
    {
        return std::hash<std::string>{}(digest.hash());
    }
};
} // namespace std

namespace build {
namespace bazel {
namespace remote {
namespace execution {
namespace v2 {
inline bool operator==(const BloombergLP::recc::proto::Digest &a,
                       const BloombergLP::recc::proto::Digest &b)
{
    return a.hash() == b.hash() && a.size_bytes() == b.size_bytes();
}
} // namespace v2
} // namespace execution
} // namespace remote
} // namespace bazel
} // namespace build
#endif
