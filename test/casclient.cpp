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
#include <env.h>
#include <fileutils.h>
#include <merklize.h>

#include <build/bazel/remote/execution/v2/remote_execution_mock.grpc.pb.h>
#include <gmock/gmock.h>
#include <google/bytestream/bytestream_mock.grpc.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <regex>

using namespace BloombergLP::recc;
using namespace testing;

const std::string abc("abc");
const std::string defg("defg");
const std::string emptyString;
const std::unordered_map<proto::Digest, std::string> emptyMap;

MATCHER_P(MessageEq, expected, "")
{
    return google::protobuf::util::MessageDifferencer::Equals(expected, arg);
}

MATCHER(HasNoBlobDigests, "") { return arg.blob_digests_size() == 0; }

MATCHER_P(HasBlobDigest, blob_digest, "")
{
    for (int i = 0; i < arg.blob_digests_size(); ++i) {
        if (arg.blob_digests(i) == blob_digest) {
            return true;
        }
    }
    return false;
}

MATCHER_P2(HasUpdateBlobRequest, digest, data, "")
{
    for (int i = 0; i < arg.requests_size(); ++i) {
        if (arg.requests(i).digest() == digest) {
            EXPECT_EQ(arg.requests(i).data(), data);
            return true;
        }
    }
    return false;
}

TEST(CASClientTest, EmptyUpload)
{
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    proto::FindMissingBlobsResponse response;

    EXPECT_CALL(*stub, FindMissingBlobs(_, HasNoBlobDigests(), _))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));
    EXPECT_CALL(*stub, BatchUpdateBlobs(_, _, _)).Times(0);

    cas.upload_resources(emptyMap, emptyMap);
}

TEST(CASClientTest, AlreadyUploadedBlob)
{
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    std::unordered_map<proto::Digest, std::string> blobs;
    blobs[make_digest(abc)] = abc;
    proto::FindMissingBlobsResponse response;

    EXPECT_CALL(*stub, FindMissingBlobs(_, HasBlobDigest(make_digest(abc)), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));
    EXPECT_CALL(*stub, BatchUpdateBlobs(_, _, _)).Times(0);

    cas.upload_resources(blobs, emptyMap);
}

TEST(CASClientTest, AlreadyUploadedFile)
{
    TemporaryDirectory tmpdir;
    std::string path = tmpdir.name() + std::string("/abc.txt");
    write_file(path.c_str(), "abc");

    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    std::unordered_map<proto::Digest, std::string> filenames;
    filenames[make_digest(abc)] = path;
    proto::FindMissingBlobsResponse response;

    EXPECT_CALL(*stub, FindMissingBlobs(_, HasBlobDigest(make_digest(abc)), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));
    EXPECT_CALL(*stub, BatchUpdateBlobs(_, _, _)).Times(0);

    cas.upload_resources(emptyMap, filenames);
}

TEST(CASClientTest, NewBlobUpload)
{
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    std::unordered_map<proto::Digest, std::string> blobs;
    blobs[make_digest(abc)] = abc;
    blobs[make_digest(defg)] = defg;
    proto::FindMissingBlobsResponse response;
    *response.add_missing_blob_digests() = make_digest(defg);

    EXPECT_CALL(*stub,
                FindMissingBlobs(_,
                                 AllOf(HasBlobDigest(make_digest(abc)),
                                       HasBlobDigest(make_digest(defg))),
                                 _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));

    proto::BatchUpdateBlobsResponse updateBlobsResponse;
    *updateBlobsResponse.add_responses()->mutable_digest() = make_digest(defg);
    EXPECT_CALL(
        *stub,
        BatchUpdateBlobs(_, HasUpdateBlobRequest(make_digest(defg), defg), _))
        .WillOnce(DoAll(SetArgPointee<2>(updateBlobsResponse),
                        Return(grpc::Status::OK)));

    cas.upload_resources(blobs, emptyMap);
}

TEST(CASClientTest, NewFileUpload)
{
    TemporaryDirectory tmpdir;
    std::string path = tmpdir.name() + std::string("/abc.txt");
    write_file(path.c_str(), "abc");

    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    std::unordered_map<proto::Digest, std::string> filenames;
    filenames[make_digest(abc)] = path;
    proto::FindMissingBlobsResponse response;
    *response.add_missing_blob_digests() = make_digest(abc);

    EXPECT_CALL(*stub, FindMissingBlobs(_, HasBlobDigest(make_digest(abc)), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));

    proto::BatchUpdateBlobsResponse updateBlobsResponse;
    *updateBlobsResponse.add_responses()->mutable_digest() = make_digest(abc);
    EXPECT_CALL(*stub, BatchUpdateBlobs(
                           _, HasUpdateBlobRequest(make_digest(abc), abc), _))
        .WillOnce(DoAll(SetArgPointee<2>(updateBlobsResponse),
                        Return(grpc::Status::OK)));

    cas.upload_resources(emptyMap, filenames);
}

ACTION_P3(AddWriteRequestData, blob, name, isComplete)
{
    EXPECT_EQ(arg0.write_offset(), blob->length());
    EXPECT_FALSE(*isComplete);
    if (name->empty()) {
        ASSERT_FALSE(arg0.resource_name().empty());
        *name = arg0.resource_name();
    }
    else if (!arg0.resource_name().empty()) {
        EXPECT_EQ(arg0.resource_name(), *name);
    }
    *blob += arg0.data();
    *isComplete = arg0.finish_write();
}

TEST(CASClientTest, LargeBlobUpload)
{
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    auto writer = new grpc::testing::MockClientWriter<
        google::bytestream::WriteRequest>();

    std::unordered_map<proto::Digest, std::string> blobs;
    std::string bigBlob(50000000, 'q');
    auto bigBlobDigest = make_digest(bigBlob);
    blobs[bigBlobDigest] = bigBlob;
    proto::FindMissingBlobsResponse response;
    *response.add_missing_blob_digests() = bigBlobDigest;

    EXPECT_CALL(*stub, FindMissingBlobs(_, HasBlobDigest(bigBlobDigest), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));

    google::bytestream::WriteResponse writeResponse;
    writeResponse.set_committed_size(bigBlob.length());

    EXPECT_CALL(*byteStreamStub, WriteRaw(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(writeResponse), Return(writer)));

    std::string storedBlob;
    std::string name;
    bool isComplete;
    EXPECT_CALL(*writer, Write(_, _))
        .WillRepeatedly(
            DoAll(AddWriteRequestData(&storedBlob, &name, &isComplete),
                  Return(true)));
    EXPECT_CALL(*writer, WritesDone()).WillOnce(Return(true));
    EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

    cas.upload_resources(blobs, emptyMap);

    EXPECT_EQ(storedBlob, bigBlob);

    std::string uploadNameRegex("uploads\\/"
                                "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{"
                                "4}-[0-9a-f]{12}\\/blobs\\/");
    uploadNameRegex += bigBlobDigest.hash();
    uploadNameRegex += "\\/";
    uploadNameRegex += std::to_string(bigBlobDigest.size_bytes());
    EXPECT_TRUE(regex_match(name, std::regex(uploadNameRegex)));
}

TEST(CASClientTest, FetchBlob)
{
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    auto digest = make_digest(abc);
    google::bytestream::ReadRequest expectedRequest;
    expectedRequest.set_resource_name("blobs/" + digest.hash() + "/3");

    google::bytestream::ReadResponse response;
    response.set_data(abc);

    auto reader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();

    EXPECT_CALL(*byteStreamStub, ReadRaw(_, MessageEq(expectedRequest)))
        .WillOnce(Return(reader));
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    auto blob = cas.fetch_blob(digest);
    EXPECT_EQ(blob, abc);
}

TEST(CASClientTest, FetchBlobResumeDownload)
{
    RECC_RETRY_LIMIT = 1;
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    auto digest = make_digest(abc);
    google::bytestream::ReadRequest firstRequest;
    firstRequest.set_resource_name("blobs/" + digest.hash() + "/3");

    google::bytestream::ReadRequest secondRequest;
    secondRequest.set_resource_name("blobs/" + digest.hash() + "/3");
    secondRequest.set_read_offset(1);

    google::bytestream::ReadResponse responseAB;
    responseAB.set_data(abc.substr(0, abc.size() / 2));
    google::bytestream::ReadResponse responseC;
    responseC.set_data(abc.substr(abc.size() / 2));

    auto brokenReader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();
    EXPECT_CALL(*brokenReader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(responseAB), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*brokenReader, Finish())
        .WillOnce(Return(
            grpc::Status(grpc::FAILED_PRECONDITION, "failing for test")));

    auto reader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(responseC), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    EXPECT_CALL(*byteStreamStub, ReadRaw(_, MessageEq(firstRequest)))
        .WillOnce(Return(brokenReader));

    EXPECT_CALL(*byteStreamStub, ReadRaw(_, MessageEq(secondRequest)))
        .WillOnce(Return(reader));

    auto blob = cas.fetch_blob(digest);
    EXPECT_EQ(blob, abc);
}

// Only tests catch block in DownloadDirectory. Verifies that missing string is
// correctly constructed
TEST(CASClientTest, DownloadDirectory)
{
    RECC_RETRY_LIMIT = 0;
    const auto directoryDigest = make_digest("fake directory");
    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    // construct proto directory
    proto::Directory upload_dir;
    proto::FileNode *upload_file = upload_dir.add_files();
    proto::Digest *file_digest = upload_file->mutable_digest();

    const int file_size = 135;
    const std::string file_hash =
        "73eb9e7f296ec5273ea74ac888dd8d1b45850a7510c0de1e83809901e585b1f5";

    file_digest->set_hash(file_hash);
    file_digest->set_size_bytes(file_size);
    upload_file->set_name("hello.cpp");
    upload_file->set_is_executable(false);

    // convert proto directory to string
    std::string serialized_dir;
    upload_dir.SerializeToString(&serialized_dir);
    std::string serialized_file;
    upload_file->SerializeToString(&serialized_file);

    // mock objects for fetch blobs
    auto reader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();

    auto file_reader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();

    google::bytestream::ReadResponse response;
    response.set_data(serialized_dir);

    google::bytestream::ReadResponse file_response;
    file_response.set_data(serialized_file);

    // both fetch blobs calls will hit same stub
    EXPECT_CALL(*byteStreamStub, ReadRaw(_, _))
        .WillOnce(Return(reader))
        .WillOnce(Return(file_reader));

    // return serial proto directory
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    // return serialized file, but set failed status
    grpc::Status status(grpc::StatusCode::NOT_FOUND, "Blob not found");
    EXPECT_CALL(*file_reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(file_response), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*file_reader, Finish()).WillOnce(Return(status));

    std::vector<std::string> missing;
    cas.download_directory(directoryDigest,
                           get_current_working_directory().c_str(), &missing);

    const std::string expected_missing =
        "blobs/" + file_hash + "/" + std::to_string(file_size);
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing[0], expected_missing);
}
