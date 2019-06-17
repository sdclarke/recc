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

#include <errno.h>
#include <stdexcept>
#include <string.h>

#include <reccmetrics/udpwriter.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {

UDPWriter::UDPWriter(int server_port, const std::string &server_name)
    : d_sockfd(0)
{
    // Initialize sockaddr_in struct
    memset(&d_server_address, 0, sizeof(d_server_address));
    d_server_address.sin_family = AF_INET;
    inet_pton(AF_INET, server_name.c_str(), &d_server_address.sin_addr);
    d_server_address.sin_port = htons(server_port);

    connect();
}

void UDPWriter::connect()
{
    if ((d_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        const std::string error_msg =
            "Could not create UDP Socket to publish metrics: " +
            std::string(strerror(errno));
        throw std::runtime_error(error_msg);
    }
}

void UDPWriter::write(const std::string &buffer)
{
    sendto(d_sockfd, buffer.c_str(), buffer.size(), 0,
           (struct sockaddr *)&d_server_address, sizeof(d_server_address));
}

UDPWriter::~UDPWriter()
{
    if (d_sockfd) {
        close(d_sockfd);
    }
}

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
