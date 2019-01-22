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

#include <random>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_set>

namespace BloombergLP {
namespace recc {

const char HEX_CHARS[] = "0123456789abcdef";

/**
 * Generate and return a random version 4 GUID.
 */
std::string generate_guid()
{
    std::random_device randomDevice;
    std::default_random_engine engine(randomDevice());
    std::uniform_int_distribution<> hexDist(0, 15);
    std::string result(36, '0');
    for (int i = 0; i < 36; ++i) {
        int num = hexDist(engine);
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

void CASClient::upload_blob(proto::Digest digest, std::string blob)
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
            for (int offset = 0; offset < blob.length();
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

    grpc_retry(write_lambda);

    if (response.committed_size() != blob.length()) {
        throw std::runtime_error("ByteStream upload failed.");
    }
}

std::string CASClient::fetch_blob(proto::Digest digest)
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
        request.set_read_offset(result.size());
        auto reader = d_byteStreamStub->Read(&context, request);
        google::bytestream::ReadResponse readResponse;
        while (reader->Read(&readResponse)) {
            result += readResponse.data();
        }
        return reader->Finish();
    };
    grpc_retry(fetch_lambda);
    return result;
}

const int MAX_TOTAL_BATCH_SIZE = 1 << 21;
const int MAX_MISSING_BLOBS_REQUEST_ITEMS = 1 << 14;

void CASClient::upload_resources(
    std::unordered_map<proto::Digest, std::string> blobs,
    std::unordered_map<proto::Digest, std::string> filenames)
{
    std::unordered_set<proto::Digest> digestsToUpload;
    for (const auto &i : blobs) {
        digestsToUpload.insert(i.first);
    }
    for (const auto &i : filenames) {
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
        RECC_LOG_VERBOSE("Sending missing blobs request: "
                         << missingBlobsRequest.ShortDebugString());
        proto::FindMissingBlobsResponse missingBlobsResponse;

        auto missing_blobs_lambda = [&](grpc::ClientContext &context) {
            missingBlobsResponse.Clear();
            return d_executionStub->FindMissingBlobs(
                &context, missingBlobsRequest, &missingBlobsResponse);
        };
        grpc_retry(missing_blobs_lambda);
        RECC_LOG_VERBOSE("Got missing blobs response: "
                         << missingBlobsResponse.ShortDebugString());
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
            blob = blobs[digest];
        }
        else if (filenames.count(digest)) {
            blob = get_file_contents(filenames[digest].c_str());
        }
        else {
            throw std::runtime_error(
                "CAS server requested nonexistent digest");
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
            grpc_retry(batch_update_lambda);

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
        proto::BatchUpdateBlobsResponse response;
        RECC_LOG_VERBOSE("Sending final batch update request");
        auto batch_update_lambda = [&](grpc::ClientContext &context) {
            response.Clear();
            return d_executionStub->BatchUpdateBlobs(
                &context, batchUpdateRequest, &response);
        };
        grpc_retry(batch_update_lambda);
        for (int i = 0; i < response.responses_size(); ++i) {
            ensure_ok(response.responses(i).status());
        }
    }
}

void CASClient::download_directory(proto::Digest digest, const char *path,
                                   std::vector<std::string> *missing)
{
    RECC_LOG_VERBOSE("Downloading directory to " << path);
    auto directory = fetch_message<proto::Directory>(digest);

    for (auto &file : directory.files()) {
        try {
            auto blob = fetch_blob(file.digest());
            const std::string filePath = std::string(path) + "/" + file.name();
            write_file(filePath.c_str(), blob);
            if (file.is_executable()) {
                make_executable(filePath.c_str());
            }
        }
        catch (const std::runtime_error &e) {
            const std::string base =
                "Retry limit exceeded. Last gRPC error was ";
            const std::string expected_err = base + "5: Blob not found";
            if (e.what() != expected_err) {
                throw;
            }
            proto::Digest digest = file.digest();
            std::ostringstream resourceName;
            resourceName << "blobs/" << digest.hash() << "/"
                         << std::to_string(digest.size_bytes());
            missing->push_back(resourceName.str());
        }
    }

    for (auto &subdirectory : directory.directories()) {
        auto subdirPath = std::string(path) + "/" + subdirectory.name();
        if (mkdir(subdirPath.c_str(), 0777) != 0) {
            if (errno != EEXIST) {
                throw std::system_error(errno, std::system_category());
            }
        }
        download_directory(subdirectory.digest(), subdirPath.c_str(), missing);
    }
}
} // namespace recc
} // namespace BloombergLP
