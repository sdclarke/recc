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

#include <casclient.h>

#include <fileutils.h>
#include <grpcretry.h>
#include <logging.h>
#include <merklize.h>
#include <reccmetrics/durationmetrictimer.h>
#include <reccmetrics/metricguard.h>

#include <random>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_set>

#define TIMER_NAME_FIND_MISSING_BLOBS "recc.find_missing_blobs"
#define TIMER_NAME_UPLOAD_MISSING_BLOBS "recc.upload_missing_blobs"

namespace BloombergLP {
namespace recc {

const char HEX_CHARS[] = "0123456789abcdef";

/**
 * Generate and return a random version 4 GUID.
 */
std::string generate_guid()
{
    const auto GUID_LENGTH = 36;
    std::string result(GUID_LENGTH, '0');

    std::random_device randomDevice;
    std::default_random_engine engine(randomDevice());
    std::uniform_int_distribution<> hexDist(0, 15);

    for (unsigned int i = 0; i < GUID_LENGTH; ++i) {
        const int num = hexDist(engine);
        switch (i) {
            case 8:
            case 13:
            case 18:
            case 23:
                result[i] = '-';
                break;
            case 14:
                result[i] = '4';
                break;
            case 19:
                result[i] = HEX_CHARS[8 | (num & 3)];
                break;
            default:
                result[i] = HEX_CHARS[hexDist(engine)];
        }
    }
    return result;
}

const std::string GUID = generate_guid();
const int BYTESTREAM_CHUNK_SIZE = 1 << 20;

void CASClient::upload_blob(const proto::Digest &digest,
                            const std::string &blob) const
{
    google::bytestream::WriteResponse response;

    std::string resourceName = d_instance;
    if (d_instance.length() > 0) {
        resourceName += "/";
    }
    resourceName += "uploads/" + GUID + "/blobs/" + digest.hash() + "/" +
                    std::to_string(digest.size_bytes());

    auto write_lambda = [&](grpc::ClientContext &context) {
        response.Clear();
        auto writer = d_byteStreamStub->Write(&context, &response);
        google::bytestream::WriteRequest initialRequest;

        initialRequest.set_resource_name(resourceName);
        initialRequest.set_write_offset(0);

        if (writer->Write(initialRequest)) {
            for (unsigned int offset = 0; offset < blob.length();
                 offset += BYTESTREAM_CHUNK_SIZE) {
                google::bytestream::WriteRequest request;
                request.set_write_offset(offset);
                request.set_finish_write((offset + BYTESTREAM_CHUNK_SIZE) >=
                                         blob.length());

                if (request.finish_write()) {
                    request.set_data(blob.c_str() + offset,
                                     blob.length() - offset);
                }
                else {
                    request.set_data(blob.c_str() + offset,
                                     BYTESTREAM_CHUNK_SIZE);
                }

                if (!writer->Write(request)) {
                    break;
                }
            }
        }
        writer->WritesDone();
        return writer->Finish();
    };

    grpc_retry(write_lambda, d_grpcContext);

    if (response.committed_size() !=
        static_cast<google::protobuf::int64>(blob.length())) {
        throw std::runtime_error("ByteStream upload failed.");
    }
}

std::string CASClient::fetch_blob(const proto::Digest &digest) const
{
    std::string resourceName = d_instance;
    if (!d_instance.empty()) {
        resourceName += "/";
    }
    resourceName += "blobs/";
    resourceName += digest.hash();
    resourceName += "/";
    resourceName += std::to_string(digest.size_bytes());

    std::string result;
    auto fetch_lambda = [&](grpc::ClientContext &context) {
        google::bytestream::ReadRequest request;
        request.set_resource_name(resourceName);
        request.set_read_offset(
            static_cast<google::protobuf::int64>(result.size()));

        auto reader = d_byteStreamStub->Read(&context, request);

        google::bytestream::ReadResponse readResponse;
        while (reader->Read(&readResponse)) {
            result += readResponse.data();
        }
        return reader->Finish();
    };

    grpc_retry(fetch_lambda, d_grpcContext);
    return result;
}

const int MAX_TOTAL_BATCH_SIZE = 1 << 21;
const int MAX_MISSING_BLOBS_REQUEST_ITEMS = 1 << 14;

void CASClient::upload_resources(
    const digest_string_umap &blobs,
    const digest_string_umap &digest_to_filecontents) const
{
    std::unordered_set<proto::Digest> digestsToUpload;
    for (const auto &i : blobs) {
        digestsToUpload.insert(i.first);
    }
    for (const auto &i : digest_to_filecontents) {
        digestsToUpload.insert(i.first);
    }

    std::unordered_set<proto::Digest> missingDigests;

    auto digestIter = digestsToUpload.cbegin();
    while (digestIter != digestsToUpload.cend()) {
        proto::FindMissingBlobsRequest missingBlobsRequest;
        missingBlobsRequest.set_instance_name(d_instance);
        while (missingBlobsRequest.blob_digests_size() <
                   MAX_MISSING_BLOBS_REQUEST_ITEMS &&
               digestIter != digestsToUpload.cend()) {
            *missingBlobsRequest.add_blob_digests() = *digestIter;
            ++digestIter;
        }

        RECC_LOG_VERBOSE(
            "Sending FindMissingBlobsRequest with a total number of blobs: "
            << missingBlobsRequest.blob_digests_size());
        proto::FindMissingBlobsResponse missingBlobsResponse;

        auto missing_blobs_lambda = [&](grpc::ClientContext &context) {
            missingBlobsResponse.Clear();
            return d_executionStub->FindMissingBlobs(
                &context, missingBlobsRequest, &missingBlobsResponse);
        };

        { // Timed block
            reccmetrics::MetricGuard<reccmetrics::DurationMetricTimer> mt(
                TIMER_NAME_FIND_MISSING_BLOBS, RECC_ENABLE_METRICS);

            grpc_retry(missing_blobs_lambda, d_grpcContext);
        }

        RECC_LOG_VERBOSE(
            "Received FindMissingBlobsResponse with a total number of blobs: "
            << missingBlobsResponse.missing_blob_digests_size());
        for (int i = 0; i < missingBlobsResponse.missing_blob_digests_size();
             ++i) {
            missingDigests.insert(
                missingBlobsResponse.missing_blob_digests(i));
        }
    }

    proto::BatchUpdateBlobsRequest batchUpdateRequest;
    batchUpdateRequest.set_instance_name(d_instance);
    int batchSize = 0;
    for (const auto &digest : missingDigests) {
        std::string blob;

        if (blobs.count(digest)) {
            blob = blobs.at(digest);
        }
        else if (digest_to_filecontents.count(digest)) {
            blob = digest_to_filecontents.at(digest);
        }
        else {
            throw std::runtime_error(
                "CAS server requested non-existent digest");
        }

        if (digest.size_bytes() > MAX_TOTAL_BATCH_SIZE) {
            upload_blob(digest, blob);
            continue;
        }
        else if (digest.size_bytes() + batchSize > MAX_TOTAL_BATCH_SIZE) {
            proto::BatchUpdateBlobsResponse response;

            auto batch_update_lambda = [&](grpc::ClientContext &context) {
                response.Clear();
                return d_executionStub->BatchUpdateBlobs(
                    &context, batchUpdateRequest, &response);
            };

            RECC_LOG_VERBOSE("Sending batch update request");
            grpc_retry(batch_update_lambda, d_grpcContext);

            for (int j = 0; j < response.responses_size(); ++j) {
                ensure_ok(response.responses(j).status());
            }

            batchUpdateRequest = proto::BatchUpdateBlobsRequest();
            batchUpdateRequest.set_instance_name(d_instance);
            batchSize = 0;
        }

        proto::BatchUpdateBlobsRequest_Request *updateRequest =
            batchUpdateRequest.add_requests();
        *updateRequest->mutable_digest() = digest;
        updateRequest->set_data(blob);
        batchSize += digest.size_bytes();
        batchSize += digest.hash().length();
    }

    if (batchUpdateRequest.requests_size() > 0) {
        // Timed block
        reccmetrics::MetricGuard<reccmetrics::DurationMetricTimer> mt(
            TIMER_NAME_UPLOAD_MISSING_BLOBS, RECC_ENABLE_METRICS);

        proto::BatchUpdateBlobsResponse response;
        RECC_LOG_VERBOSE("Sending final batch update request");

        auto batch_update_lambda = [&](grpc::ClientContext &context) {
            response.Clear();
            return d_executionStub->BatchUpdateBlobs(
                &context, batchUpdateRequest, &response);
        };

        grpc_retry(batch_update_lambda, d_grpcContext);

        for (int i = 0; i < response.responses_size(); ++i) {
            ensure_ok(response.responses(i).status());
        }
    }
}

} // namespace recc
} // namespace BloombergLP
