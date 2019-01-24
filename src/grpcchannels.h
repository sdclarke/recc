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

#include <grpcpp/security/credentials.h>

class GrpcChannels {
  public:
    typedef std::shared_ptr<grpc::Channel> ChannelPtr;

    /**
     * builds appropriate channels from environment
     * variables. Will return a channel for cas, and
     * a channel for the build server.
     */
    static GrpcChannels get_channels_from_config();

    ChannelPtr server() { return d_server; }
    ChannelPtr cas() { return d_cas; }

  private:
    /*
     * Left private as this object should be constructed using
     * 'get_channels_from_config'.
     */
    GrpcChannels(const ChannelPtr &server, const ChannelPtr &cas)
        : d_server(server), d_cas(cas)
    {
    }

    ChannelPtr d_server;
    ChannelPtr d_cas;
};
