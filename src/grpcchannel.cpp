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
#include <env.h>
#include <grpcchannel.h>

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

using namespace BloombergLP::recc;

grpcChannels get_channels_from_config()
{
    if (RECC_SERVER_AUTH_GOOGLEAPI && RECC_SERVER_SSL) {
        throw std::invalid_argument("RECC_SERVER_AUTH_GOOGLEAPI & "
                                    "RECC_SERVER_SSL cannot both be set.");
    }
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (RECC_SERVER_AUTH_GOOGLEAPI) {
        creds = grpc::GoogleDefaultCredentials();
    }
    else if (RECC_SERVER_SSL) {
        grpc::SslCredentialsOptions options;
        creds = grpc::SslCredentials(options);
    }
    else {
        creds = grpc::InsecureChannelCredentials();
    }
    grpcChannels return_channels;
    return_channels.server = grpc::CreateChannel(RECC_SERVER, creds);
    return_channels.cas = grpc::CreateChannel(RECC_CAS_SERVER, creds);
    return return_channels;
}
