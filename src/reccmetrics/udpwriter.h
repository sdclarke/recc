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

#ifndef INCLUDED_RECCMETRICS_UDPWRITER_H
#define INCLUDED_RECCMETRICS_UDPWRITER_H

#include <arpa/inet.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {
/**
 * UDPWriter
 */
class UDPWriter {
  private:
    struct sockaddr_in d_server_address;
    int d_sockfd;

    void connect();

  public:
    explicit UDPWriter(int port, const std::string &server_name = "127.0.0.1");

    void write(const std::string &buffer);

    ~UDPWriter();
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
