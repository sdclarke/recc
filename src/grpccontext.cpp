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
//

#include <grpccontext.h>
#include <requestmetadata.h>

#include <grpcpp/security/credentials.h>

namespace BloombergLP {
namespace recc {

GrpcContext::GrpcClientContextPtr GrpcContext::new_client_context()
{
    GrpcContext::GrpcClientContextPtr context(
        std::make_unique<grpc::ClientContext>());

    RequestMetadataGenerator::attach_request_metadata(*context, d_action_id);
    return context;
}

void GrpcContext::set_action_id(const std::string &action_id)
{
    d_action_id = action_id;
}

} // namespace recc
} // namespace BloombergLP
