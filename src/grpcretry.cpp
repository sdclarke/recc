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

#include <grpcretry.h>

#include <env.h>
#include <logging.h>

#include <math.h>
#include <thread>

namespace BloombergLP {
namespace recc {

void grpc_retry(
    const std::function<grpc::Status(grpc::ClientContext &)> &grpc_invocation)
{
    int n_attempts = 0;
    grpc::Status status;
    do {
        grpc::ClientContext context;
        status = grpc_invocation(context);
        if (status.ok()) {
            return;
        }

        /* The call failed. */
        if (n_attempts < RECC_RETRY_LIMIT) {
            /* Delay the next call based on the number of attempts made */
            int time_delay =
                RECC_RETRY_DELAY * pow(static_cast<double>(2), n_attempts);

            std::string error_msg =
                "Attempt " + std::to_string(n_attempts + 1) + "/" +
                std::to_string(RECC_RETRY_LIMIT + 1) +
                " failed with gRPC error " +
                std::to_string(status.error_code()) + ": " +
                status.error_message() + ". Retrying in " +
                std::to_string(time_delay) + " ms...";

            RECC_LOG_ERROR(error_msg);

            std::this_thread::sleep_for(std::chrono::milliseconds(time_delay));
        }

        n_attempts++;

    } while (n_attempts < RECC_RETRY_LIMIT + 1);

    throw std::runtime_error("Retry limit exceeded. Last gRPC error was " +
                             std::to_string(status.error_code()) + ": " +
                             status.error_message());
}

} // namespace recc
} // namespace BloombergLP
