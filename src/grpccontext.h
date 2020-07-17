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

#ifndef INCLUDED_GRPCCONTEXT
#define INCLUDED_GRPCCONTEXT

#include <grpcpp/client_context.h>

namespace BloombergLP {
namespace recc {

class GrpcContext {
  public:
    typedef std::unique_ptr<grpc::ClientContext> GrpcClientContextPtr;

    GrpcContext() {}

    /**
     * Build a new ClientContext object for rpc calls.
     */
    GrpcClientContextPtr new_client_context();

    /**
     * Set `RequestMetadata.action_id` value to attach to request headers.
     */
    void set_action_id(const std::string &action_id);

  private:
    std::string d_action_id;
};

} // namespace recc
} // namespace BloombergLP

#endif
