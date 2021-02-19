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

#include <casclient.h>
#include <digestgenerator.h>
#include <hashtohex.h>

#include <buildboxcommon_logging.h>
#include <buildboxcommonmetrics_durationmetrictimer.h>
#include <buildboxcommonmetrics_metricguard.h>
#include <grpcretry.h>

#include <random>
#include <sstream>
#include <unordered_set>

#define TIMER_NAME_FIND_MISSING_BLOBS "recc.find_missing_blobs"
#define TIMER_NAME_UPLOAD_MISSING_BLOBS "recc.upload_missing_blobs"

namespace BloombergLP {
namespace recc {

const std::string CASClient::s_guid = generate_guid();

const int CASClient::s_byteStreamChunkSizeBytes = 1 * 1024 * 1024;
const int CASClient::s_maxTotalBatchSizeBytes = 2 * 1024 * 1024;
const int CASClient::s_maxMissingBlobsRequestItems = 16384;

CASClient::CASClient(
    std::shared_ptr<proto::ContentAddressableStorage::StubInterface>
        executionStub,
    std::shared_ptr<google::bytestream::ByteStream::StubInterface>
        byteStreamStub,
    std::shared_ptr<proto::Capabilities::StubInterface> capabilitiesStub,
    const std::string &instanceName, GrpcContext *grpcContext)
    : d_executionStub(executionStub), d_byteStreamStub(byteStreamStub),
      d_capabilitiesStub(capabilitiesStub), d_instanceName(instanceName),
      d_grpcContext(grpcContext)
{
}

CASClient::CASClient(std::shared_ptr<grpc::Channel> channel,
                     const std::string &instanceName, GrpcContext *grpcContext)
    : d_executionStub(proto::ContentAddressableStorage::NewStub(channel)),
      d_byteStreamStub(google::bytestream::ByteStream::NewStub(channel)),
      d_capabilitiesStub(proto::Capabilities::NewStub(channel)),
      d_instanceName(instanceName), d_grpcContext(grpcContext)
{
}

/**
 * Generate and return a random version 4 GUID.
 */
std::string CASClient::generate_guid()
{
    static const std::string hexChars = "0123456789abcdef";

    const auto guidLength = 36;
    std::string result(guidLength, '0');

    std::random_device randomDevice;
    std::default_random_engine engine(randomDevice());
    std::uniform_int_distribution<> hexDist(0, 15);

    for (unsigned int i = 0; i < guidLength; ++i) {
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
                result[i] = hexChars[8 | (num & 3)];
                break;
            default:
                const auto char_idx = static_cast<size_t>(hexDist(engine));
                result[i] = hexChars[char_idx];
        }
    }
    return result;
}

int64_t CASClient::maxTotalBatchSizeBytes() const
{
    return d_maxTotalBatchSizeBytes;
}

void CASClient::setUpFromServerCapabilities()
{
    proto::ServerCapabilities serverCapabilities;
    try {
        serverCapabilities = fetchServerCapabilities();
    }
    catch (const std::runtime_error &e) {
        BUILDBOX_LOG_DEBUG(
            "Error: Could not fetch capabilities, using defaults: "
            << e.what());
        return;
    }

    const auto cache_capabilities = serverCapabilities.cache_capabilities();

    // Maximum bytes that can be batched in a request:
    const int64_t serverMaxBatchTotalSizeBytes =
        cache_capabilities.max_batch_total_size_bytes();

    // If the server specifies a smaller limit than the one we currently have,
    // we override it. (0 means the server does not impose any limit).
    if (serverMaxBatchTotalSizeBytes > 0 &&
        serverMaxBatchTotalSizeBytes < d_maxTotalBatchSizeBytes) {
        d_maxTotalBatchSizeBytes = serverMaxBatchTotalSizeBytes;
    }

    // Checking that the function that we are using is supported by the server:
    const auto configured_digest_function =
        DigestGenerator::stringToDigestFunctionMap().at(
            RECC_CAS_DIGEST_FUNCTION);

    const auto supported_functions = cache_capabilities.digest_function();

    if (std::find(supported_functions.cbegin(), supported_functions.cend(),
                  configured_digest_function) == supported_functions.cend()) {
        const std::string error_message =
            "CAS server does not support the configured digest function: " +
            RECC_CAS_DIGEST_FUNCTION;

        BUILDBOX_LOG_ERROR(error_message);
        throw std::runtime_error(error_message);
    }
}

proto::ServerCapabilities CASClient::fetchServerCapabilities() const
{
    proto::ServerCapabilities serverCapabilities;

    auto getCapabilitiesLambda = [&](grpc::ClientContext &context) {
        proto::GetCapabilitiesRequest request;
        request.set_instance_name(d_instanceName);

        return this->d_capabilitiesStub->GetCapabilities(&context, request,
                                                         &serverCapabilities);
    };

    grpc_retry(getCapabilitiesLambda, d_grpcContext);
    return serverCapabilities;
}

//std::string hashToHex(const unsigned char *hash_buffer, unsigned int hash_size)
//{
    //std::ostringstream ss;
    //for (unsigned int i = 0; i < hash_size; i++) {
        //ss << std::hex << std::setw(2) << std::setfill('0')
           //<< static_cast<int>(hash_buffer[i]);
    //}
    //return ss.str();
//}


std::string CASClient::uploadResourceName(const proto::Digest &digest) const
{
    std::string resourceName = this->d_instanceName;
    if (!resourceName.empty()) {
        resourceName += "/";
    }

    if (digest.hash_other().length() == 0) {
      resourceName += "uploads/" + s_guid + "/blobs/B3Z:" + hashToHex((const unsigned char*)digest.hash_blake3zcc().c_str(), static_cast<unsigned int>(32)) + "/" + std::to_string(digest.size_bytes());
    }
    else {
      resourceName += "uploads/" + s_guid + "/blobs/" + digest.hash_other() + "/" +
        std::to_string(digest.size_bytes());
    }

    return resourceName;
}

std::string CASClient::downloadResourceName(const proto::Digest &digest) const
{
    std::string resourceName = this->d_instanceName;
    if (!resourceName.empty()) {
        resourceName += "/";
    }

    if (digest.hash_other().length() == 0) {
      resourceName +=
        "blobs/B3Z:" + hashToHex((const unsigned char*)digest.hash_blake3zcc().c_str(), static_cast<unsigned int>(32)) + "/" + std::to_string(digest.size_bytes());
    } else {
      resourceName +=
        "blobs/" + digest.hash_other() + "/" + std::to_string(digest.size_bytes());
    }
    return resourceName;
}

void CASClient::upload_blob(const proto::Digest &digest,
                            const std::string &blob) const
{
    const auto resourceName = uploadResourceName(digest);

    google::bytestream::WriteResponse response;
    auto write_lambda = [&](grpc::ClientContext &context) {
        response.Clear();

        auto writer = d_byteStreamStub->Write(&context, &response);

        google::bytestream::WriteRequest initialRequest;
        initialRequest.set_resource_name(resourceName);
        initialRequest.set_write_offset(0);

        if (writer->Write(initialRequest)) {
            for (unsigned int offset = 0; offset < blob.size();
                 offset += s_byteStreamChunkSizeBytes) {
                google::bytestream::WriteRequest request;

                size_t bytesToWrite;
                if ((offset + s_byteStreamChunkSizeBytes) >= blob.size()) {
                    bytesToWrite = blob.size() - offset;
                    request.set_finish_write(true);
                }
                else {
                    bytesToWrite = s_byteStreamChunkSizeBytes;
                    request.set_finish_write(false);
                }
                request.set_write_offset(offset);
                request.set_data(blob.c_str() + offset, bytesToWrite);

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
        static_cast<google::protobuf::int64>(blob.size())) {
        throw std::runtime_error("ByteStream upload failed.");
    }
}

std::string CASClient::fetch_blob(const proto::Digest &digest) const
{
    const auto resourceName = downloadResourceName(digest);
    std::cout << resourceName << std::endl;

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

proto::FindMissingBlobsResponse CASClient::findMissingBlobs(
    const proto::FindMissingBlobsRequest &request) const
{
    proto::FindMissingBlobsResponse response;

    BUILDBOX_LOG_DEBUG(
        "Sending FindMissingBlobsRequest with a total number of blobs: "
        << request.blob_digests_size());

    auto missing_blobs_lambda = [&](grpc::ClientContext &context) {
        return d_executionStub->FindMissingBlobs(&context, request, &response);
    };

    { // Timed block
        buildboxcommon::buildboxcommonmetrics::MetricGuard<
            buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
            mt(TIMER_NAME_FIND_MISSING_BLOBS);

        grpc_retry(missing_blobs_lambda, d_grpcContext);
    }

    BUILDBOX_LOG_DEBUG(
        "Received FindMissingBlobsResponse with a total number of blobs: "
        << response.missing_blob_digests_size());

    return response;
}

std::unordered_set<std::string> CASClient::findMissingBlobs(
    const std::unordered_set<std::string> &digests) const
{
    std::unordered_set<std::string> missingDigests;

    auto digestIter = digests.cbegin();
    while (digestIter != digests.cend()) {
        proto::FindMissingBlobsRequest missingBlobsRequest;
        missingBlobsRequest.set_instance_name(d_instanceName);

        // Filling the request with as many digests as possible...
        while (missingBlobsRequest.blob_digests_size() <
                   s_maxMissingBlobsRequestItems &&
               digestIter != digests.cend()) {
            proto::Digest d;
            d.ParseFromString(*digestIter);
            *missingBlobsRequest.add_blob_digests() = d;
            ++digestIter;
        }

        const proto::FindMissingBlobsResponse missingBlobsResponse =
            findMissingBlobs(missingBlobsRequest);

        //missingDigests.insert(
            //missingBlobsResponse.missing_blob_digests().cbegin(),
            //missingBlobsResponse.missing_blob_digests().cend());
        auto missingDigestIter = missingBlobsResponse.missing_blob_digests().cbegin();
        while (missingDigestIter != missingBlobsResponse.missing_blob_digests().cend()) {
          missingDigests.insert((*missingDigestIter).SerializeAsString());
          ++missingDigestIter;
        }
    }

    return missingDigests;
}

proto::BatchUpdateBlobsResponse CASClient::batchUpdateBlobs(
    const proto::BatchUpdateBlobsRequest &request) const
{
    proto::BatchUpdateBlobsResponse response;

    auto batch_update_lambda = [&](grpc::ClientContext &context) {
        return d_executionStub->BatchUpdateBlobs(&context, request, &response);
    };

    grpc_retry(batch_update_lambda, d_grpcContext);

    for (int j = 0; j < response.responses_size(); ++j) {
        ensure_ok(response.responses(j).status());
    }

    return response;
}

void CASClient::batchUpdateBlobs(
    const std::unordered_set<std::string> &digests,
    const digest_string_umap &blobs,
    const digest_string_umap &digest_to_filecontents) const
{
    proto::BatchUpdateBlobsRequest batchUpdateRequest;
    batchUpdateRequest.set_instance_name(d_instanceName);
    // Timed block
    buildboxcommon::buildboxcommonmetrics::MetricGuard<
        buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
        mt(TIMER_NAME_UPLOAD_MISSING_BLOBS);

    size_t batchSize = 0;
    for (const auto &digest : digests) {
        // Finding the data in one of the source maps:
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

        // If the blob is too large to batch we must upload it individually
        // using the ByteStream API:
        proto::Digest d;
        d.ParseFromString(digest);
        if (d.size_bytes() > s_maxTotalBatchSizeBytes) {
            upload_blob(d, blob);
            continue;
        }

        if (d.size_bytes() + batchSize > s_maxTotalBatchSizeBytes) {
            // Batch is full, flushing the request:
            BUILDBOX_LOG_DEBUG("Sending batch update request");
            batchUpdateBlobs(batchUpdateRequest);

            batchUpdateRequest.clear_requests();
            batchSize = 0;
        }

        proto::BatchUpdateBlobsRequest_Request *updateRequest =
            batchUpdateRequest.add_requests();
        *updateRequest->mutable_digest() = d;
        updateRequest->set_data(blob);

        batchSize += d.size_bytes();
        batchSize += static_cast<size_t>(d.hash_other().size());
        batchSize += static_cast<size_t>(d.hash_blake3zcc().size());
    }

    if (!batchUpdateRequest.requests().empty()) {
        BUILDBOX_LOG_DEBUG("Sending final update request");
        batchUpdateBlobs(batchUpdateRequest);
    }
}

void CASClient::upload_resources(
    const digest_string_umap &blobs,
    const digest_string_umap &digest_to_filecontents) const
{
    std::unordered_set<std::string> digestsToUpload;
    for (const auto &i : blobs) {
        digestsToUpload.insert(i.first);
    }
    for (const auto &i : digest_to_filecontents) {
        digestsToUpload.insert(i.first);
    }

    const auto missingDigests = findMissingBlobs(digestsToUpload);
    batchUpdateBlobs(missingDigests, blobs, digest_to_filecontents);
}

} // namespace recc
} // namespace BloombergLP
