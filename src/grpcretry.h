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

#include <functional>
#include <protos.h>

namespace BloombergLP {
namespace recc {

/**
 * Call a GRPC method. On failure, retry up to RECC_RETRY_LIMIT times,
 * using binary exponential backoff to delay between calls.
 *
 * As input, takes a function that takes a grpc::ClientContext and returns a
 * grpc::Status.
 *
 */
void grpc_retry(
    const std::function<grpc::Status(grpc::ClientContext &)> &grpc_invocation);

} // namespace recc
} // namespace BloombergLP
