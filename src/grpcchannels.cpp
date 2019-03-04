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

#include <authsession.h>
#include <env.h>
#include <grpcchannels.h>

#include <grpcpp/create_channel.h>
#include <sstream>
#include <string>

namespace BloombergLP {
namespace recc {

namespace {

std::shared_ptr<grpc::ChannelCredentials> get_channel_creds()
{
    return grpc::SslCredentials(grpc::SslCredentialsOptions());
}

} // Unnamed namespace

GrpcChannels GrpcChannels::get_channels_from_config()
{
    if (RECC_SERVER_AUTH_GOOGLEAPI && (RECC_SERVER_SSL || RECC_SERVER_JWT)) {
        throw std::invalid_argument(
            "RECC_SERVER_AUTH_GOOGLEAPI & "
            "RECC_SERVER_SSL/RECC_SERVER_JWT cannot both be set.");
    }
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (RECC_SERVER_AUTH_GOOGLEAPI) {
        creds = grpc::GoogleDefaultCredentials();
    }

    else if (RECC_SERVER_SSL || RECC_SERVER_JWT) {
        creds = get_channel_creds();
    }
    else {
        creds = grpc::InsecureChannelCredentials();
    }
    return GrpcChannels(grpc::CreateChannel(RECC_SERVER, creds),
                        grpc::CreateChannel(RECC_CAS_SERVER, creds),
                        grpc::CreateChannel(RECC_ACTION_CACHE_SERVER, creds));
}

} // namespace recc
} // namespace BloombergLP
