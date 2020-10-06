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
#include <grpcchannels.h>

#include <sstream>
#include <string>

namespace BloombergLP {
namespace recc {

GrpcChannels GrpcChannels::get_channels_from_config()
{
    buildboxcommon::ConnectionOptions connection_options_server;
    buildboxcommon::ConnectionOptions connection_options_cas;
    buildboxcommon::ConnectionOptions connection_options_action_cache;

    connection_options_server.setUrl(RECC_SERVER);
    connection_options_cas.setUrl(RECC_CAS_SERVER);
    connection_options_action_cache.setUrl(RECC_ACTION_CACHE_SERVER);

    connection_options_server.setInstanceName(RECC_INSTANCE);
    connection_options_cas.setInstanceName(RECC_INSTANCE);
    connection_options_action_cache.setInstanceName(RECC_INSTANCE);

    const std::string retryLimitStr = std::to_string(RECC_RETRY_LIMIT);
    connection_options_server.setRetryLimit(retryLimitStr);
    connection_options_cas.setRetryLimit(retryLimitStr);
    connection_options_action_cache.setRetryLimit(retryLimitStr);

    const std::string retryDelayStr = std::to_string(RECC_RETRY_DELAY);
    connection_options_server.setRetryDelay(retryDelayStr);
    connection_options_cas.setRetryDelay(retryDelayStr);
    connection_options_action_cache.setRetryDelay(retryDelayStr);

    if (RECC_ACCESS_TOKEN_PATH.size()) {
        connection_options_server.setAccessTokenPath(RECC_ACCESS_TOKEN_PATH);
        connection_options_cas.setAccessTokenPath(RECC_ACCESS_TOKEN_PATH);
        connection_options_action_cache.setAccessTokenPath(
            RECC_ACCESS_TOKEN_PATH);
    }

    if (RECC_SERVER_AUTH_GOOGLEAPI) {
        connection_options_server.setUseGoogleApiAuth(true);
        connection_options_cas.setUseGoogleApiAuth(true);
        connection_options_action_cache.setUseGoogleApiAuth(true);
    }

    return GrpcChannels(connection_options_server.createChannel(),
                        connection_options_cas.createChannel(),
                        connection_options_action_cache.createChannel());
}

} // namespace recc
} // namespace BloombergLP
