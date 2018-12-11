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
#include <logging.h>
#include <merklize.h>

#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_set>

using namespace std;

namespace BloombergLP {
namespace recc {

const char HEX_CHARS[] = "0123456789abcdef";

/**
 * Generate and return a random version 4 GUID.
 */
string generate_guid()
{
    random_device randomDevice;
    default_random_engine engine(randomDevice());
    uniform_int_distribution<> hexDist(0, 15);
    string result(36, '0');
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

const string GUID = generate_guid();
const int BYTESTREAM_CHUNK_SIZE = 1 << 20;

void CASClient::upload_blob(proto::Digest digest, string blob)
{
    grpc::ClientContext context;
    google::bytestream::WriteResponse response;
    auto writer = d_byteStreamStub->Write(&context, &response);
    google::bytestream::WriteRequest initialRequest;
    string resourceName = d_instance;
    if (d_instance.length() > 0) {
        resourceName += "/";
    }
    resourceName += "uploads/" + GUID + "/blobs/" + digest.hash() + "/" +
                    to_string(digest.size_bytes());
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
                request.set_data(blob.c_str() + offset, BYTESTREAM_CHUNK_SIZE);
            }
            if (!writer->Write(request)) {
                break;
            }
        }
    }
    writer->WritesDone();
    ensure_ok(writer->Finish());
    if (response.committed_size() != blob.length()) {
        throw runtime_error("ByteStream upload failed.");
    }
}

string CASClient::fetch_blob(proto::Digest digest)
{
    string resourceName = d_instance;
    if (!d_instance.empty()) {
        resourceName += "/";
    }
    resourceName += "blobs/";
    resourceName += digest.hash();
    resourceName += "/";
    resourceName += to_string(digest.size_bytes());

    google::bytestream::ReadRequest request;
    request.set_resource_name(resourceName);

    grpc::ClientContext context;
    auto reader = d_byteStreamStub->Read(&context, request);
    string result;
    google::bytestream::ReadResponse readResponse;
    while (reader->Read(&readResponse)) {
        result += readResponse.data();
    }
    ensure_ok(reader->Finish());
    return result;
}

const int MAX_TOTAL_BATCH_SIZE = 1 << 21;
const int MAX_MISSING_BLOBS_REQUEST_ITEMS = 1 << 14;

void CASClient::upload_resources(
    unordered_map<proto::Digest, string> blobs,
    unordered_map<proto::Digest, string> filenames)
{
    unordered_set<proto::Digest> digestsToUpload;
    for (const auto &i : blobs) {
        digestsToUpload.insert(i.first);
    }
    for (const auto &i : filenames) {
        digestsToUpload.insert(i.first);
    }

    unordered_set<proto::Digest> missingDigests;
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
        grpc::ClientContext missingBlobsContext;
        proto::FindMissingBlobsResponse missingBlobsResponse;
        ensure_ok(d_executionStub->FindMissingBlobs(
            &missingBlobsContext, missingBlobsRequest, &missingBlobsResponse));
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
        string blob;
        if (blobs.count(digest)) {
            blob = blobs[digest];
        }
        else if (filenames.count(digest)) {
            blob = get_file_contents(filenames[digest].c_str());
        }
        else {
            throw runtime_error("CAS server requested nonexistent digest");
        }

        if (digest.size_bytes() > MAX_TOTAL_BATCH_SIZE) {
            upload_blob(digest, blob);
            continue;
        }
        else if (digest.size_bytes() + batchSize > MAX_TOTAL_BATCH_SIZE) {
            grpc::ClientContext context;
            proto::BatchUpdateBlobsResponse response;
            RECC_LOG_VERBOSE("Sending batch update request");
            ensure_ok(d_executionStub->BatchUpdateBlobs(
                &context, batchUpdateRequest, &response));
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
        grpc::ClientContext context;
        proto::BatchUpdateBlobsResponse response;
        RECC_LOG_VERBOSE("Sending final batch update request");
        ensure_ok(d_executionStub->BatchUpdateBlobs(
            &context, batchUpdateRequest, &response));
        for (int i = 0; i < response.responses_size(); ++i) {
            ensure_ok(response.responses(i).status());
        }
    }
}

void CASClient::download_directory(proto::Digest digest, const char *path)
{
    RECC_LOG_VERBOSE("Downloading directory to " << path);
    auto directory = fetch_message<proto::Directory>(digest);

    for (auto &file : directory.files()) {
        auto blob = fetch_blob(file.digest());
        string filePath = string(path) + "/" + file.name();
        write_file(filePath.c_str(), blob);
        if (file.is_executable()) {
            make_executable(filePath.c_str());
        }
    }

    for (auto &subdirectory : directory.directories()) {
        auto subdirPath = string(path) + "/" + subdirectory.name();
        if (mkdir(subdirPath.c_str(), 0777) != 0) {
            if (errno != EEXIST) {
                throw system_error(errno, system_category());
            }
        }
        download_directory(subdirectory.digest(), subdirPath.c_str());
    }
}
} // namespace recc
} // namespace BloombergLP
