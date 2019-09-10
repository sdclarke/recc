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

#include <digestgenerator.h>

#include <env.h>
#include <reccmetrics/metricguard.h>
#include <reccmetrics/totaldurationmetrictimer.h>

#include <openssl/evp.h>

#define TIMER_NAME_CALCULATE_DIGESTS_TOTAL "recc.calculate_digests_total"

namespace BloombergLP {
namespace recc {

proto::Digest DigestGenerator::make_digest(const std::string &blob)
{
    static const auto hashAlgorithm = EVP_sha256();
    static const auto hashSize =
        static_cast<size_t>(EVP_MD_size(hashAlgorithm));

    static const std::string hex_digits("0123456789abcdef");

    std::string hashHex(static_cast<size_t>(hashSize) * 2, '\0');

    { // Timed block
        reccmetrics::MetricGuard<reccmetrics::TotalDurationMetricTimer> mt(
            TIMER_NAME_CALCULATE_DIGESTS_TOTAL, RECC_ENABLE_METRICS);

        // Calculate the hash.
        auto hashContext = EVP_MD_CTX_create();
        EVP_DigestInit(hashContext, hashAlgorithm);
        EVP_DigestUpdate(hashContext, &blob[0], blob.length());

        // Store the hash in an unsigned char array.
        const auto hash = std::make_unique<unsigned char[]>(hashSize);
        EVP_DigestFinal_ex(hashContext, hash.get(), nullptr);
        EVP_MD_CTX_destroy(hashContext);

        // Convert the hash to hexadecimal.
        for (unsigned int i = 0; i < hashSize; ++i) {
            hashHex[i * 2] = hex_digits[hash[i] >> 4];
            hashHex[i * 2 + 1] = hex_digits[hash[i] & 0xF];
        }
    }

    proto::Digest result;
    result.set_hash(hashHex);
    result.set_size_bytes(static_cast<google::protobuf::int64>(blob.size()));
    return result;
}

proto::Digest
DigestGenerator::make_digest(const google::protobuf::MessageLite &message)
{
    return make_digest(message.SerializeAsString());
}

} // namespace recc
} // namespace BloombergLP
