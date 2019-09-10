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
#include <grpccontext.h>
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

class CasClientFixture : public ::testing::Test {
  protected:
    const std::string instanceName;

    std::shared_ptr<proto::MockContentAddressableStorageStub> casStub;
    std::shared_ptr<google::bytestream::MockByteStreamStub> byteStreamStub;
    std::shared_ptr<proto::MockCapabilitiesStub> capabilitiesStub;

    GrpcContext grpcContext;
    CASClient casClient;

    CasClientFixture()
        : instanceName(""),
          casStub(
              std::make_shared<proto::MockContentAddressableStorageStub>()),
          byteStreamStub(
              std::make_shared<google::bytestream::MockByteStreamStub>()),
          capabilitiesStub(std::make_shared<proto::MockCapabilitiesStub>()),
          casClient(casStub, byteStreamStub, capabilitiesStub, instanceName,
                    &grpcContext)
    {
    }
};

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

TEST_F(CasClientFixture, EmptyUpload)
{
    proto::FindMissingBlobsResponse response;

    EXPECT_CALL(*casStub, FindMissingBlobs(_, HasNoBlobDigests(), _))
        .Times(AtMost(1))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));
    EXPECT_CALL(*casStub, BatchUpdateBlobs(_, _, _)).Times(0);

    casClient.upload_resources({}, {});
}

TEST_F(CasClientFixture, AlreadyUploadedBlob)
{
    digest_string_umap blobs;
    blobs[make_digest(abc)] = abc;
    proto::FindMissingBlobsResponse response;

    EXPECT_CALL(*casStub,
                FindMissingBlobs(_, HasBlobDigest(make_digest(abc)), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));
    EXPECT_CALL(*casStub, BatchUpdateBlobs(_, _, _)).Times(0);

    casClient.upload_resources(blobs, {});
}

TEST_F(CasClientFixture, AlreadyUploadedFile)
{
    TemporaryDirectory tmpdir;
    std::string path = tmpdir.name() + std::string("/abc.txt");
    write_file(path.c_str(), "abc");

    digest_string_umap digest_to_filecontents;
    digest_to_filecontents[make_digest(abc)] = path;
    proto::FindMissingBlobsResponse response;

    EXPECT_CALL(*casStub,
                FindMissingBlobs(_, HasBlobDigest(make_digest(abc)), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));
    EXPECT_CALL(*casStub, BatchUpdateBlobs(_, _, _)).Times(0);

    casClient.upload_resources({}, digest_to_filecontents);
}

TEST_F(CasClientFixture, NewBlobUpload)
{
    digest_string_umap blobs;
    blobs[make_digest(abc)] = abc;
    blobs[make_digest(defg)] = defg;
    proto::FindMissingBlobsResponse response;
    *response.add_missing_blob_digests() = make_digest(defg);

    EXPECT_CALL(*casStub,
                FindMissingBlobs(_,
                                 AllOf(HasBlobDigest(make_digest(abc)),
                                       HasBlobDigest(make_digest(defg))),
                                 _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));

    proto::BatchUpdateBlobsResponse updateBlobsResponse;
    *updateBlobsResponse.add_responses()->mutable_digest() = make_digest(defg);
    EXPECT_CALL(
        *casStub,
        BatchUpdateBlobs(_, HasUpdateBlobRequest(make_digest(defg), defg), _))
        .WillOnce(DoAll(SetArgPointee<2>(updateBlobsResponse),
                        Return(grpc::Status::OK)));

    casClient.upload_resources(blobs, {});
}

TEST_F(CasClientFixture, NewFileUpload)
{
    TemporaryDirectory tmpdir;
    std::string path = tmpdir.name() + std::string("/abc.txt");
    write_file(path.c_str(), "abc");

    digest_string_umap digest_to_filecontents;
    digest_to_filecontents[make_digest(abc)] = get_file_contents(path.c_str());
    proto::FindMissingBlobsResponse response;
    *response.add_missing_blob_digests() = make_digest(abc);

    EXPECT_CALL(*casStub,
                FindMissingBlobs(_, HasBlobDigest(make_digest(abc)), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));

    proto::BatchUpdateBlobsResponse updateBlobsResponse;
    *updateBlobsResponse.add_responses()->mutable_digest() = make_digest(abc);
    EXPECT_CALL(
        *casStub,
        BatchUpdateBlobs(_, HasUpdateBlobRequest(make_digest(abc), abc), _))
        .WillOnce(DoAll(SetArgPointee<2>(updateBlobsResponse),
                        Return(grpc::Status::OK)));

    casClient.upload_resources({}, digest_to_filecontents);
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

TEST_F(CasClientFixture, LargeBlobUpload)
{
    auto writer = new grpc::testing::MockClientWriter<
        google::bytestream::WriteRequest>();

    digest_string_umap blobs;
    std::string bigBlob(50000000, 'q');
    auto bigBlobDigest = make_digest(bigBlob);
    blobs[bigBlobDigest] = bigBlob;
    proto::FindMissingBlobsResponse response;
    *response.add_missing_blob_digests() = bigBlobDigest;

    EXPECT_CALL(*casStub, FindMissingBlobs(_, HasBlobDigest(bigBlobDigest), _))
        .WillOnce(DoAll(SetArgPointee<2>(response), Return(grpc::Status::OK)));

    google::bytestream::WriteResponse writeResponse;
    writeResponse.set_committed_size(
        static_cast<google::protobuf::int64>(bigBlob.length()));

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

    casClient.upload_resources(blobs, {});

    EXPECT_EQ(storedBlob, bigBlob);

    std::string uploadNameRegex("uploads\\/"
                                "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{"
                                "4}-[0-9a-f]{12}\\/blobs\\/");
    uploadNameRegex += bigBlobDigest.hash();
    uploadNameRegex += "\\/";
    uploadNameRegex += std::to_string(bigBlobDigest.size_bytes());
    EXPECT_TRUE(regex_match(name, std::regex(uploadNameRegex)));
}

TEST_F(CasClientFixture, FetchBlob)
{
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

    auto blob = casClient.fetch_blob(digest);
    EXPECT_EQ(blob, abc);
}

TEST_F(CasClientFixture, FetchBlobResumeDownload)
{
    RECC_RETRY_LIMIT = 1;

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

    auto blob = casClient.fetch_blob(digest);
    EXPECT_EQ(blob, abc);
}

TEST_F(CasClientFixture, FetchCapabilities)
{
    const auto maxBatchSize = 123;

    proto::CacheCapabilities cacheCapabilities;
    cacheCapabilities.set_max_batch_total_size_bytes(maxBatchSize);

    proto::ServerCapabilities serverCapabilities;
    serverCapabilities.mutable_cache_capabilities()->CopyFrom(
        cacheCapabilities);

    EXPECT_CALL(*capabilitiesStub, GetCapabilities(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(serverCapabilities),
                        Return(grpc::Status::OK)));

    casClient.setUpFromServerCapabilities();

    ASSERT_EQ(casClient.maxTotalBatchSizeBytes(), maxBatchSize);
}

TEST_F(CasClientFixture, FetchCapabilitiesFail)
{
    // Failing after retries:
    EXPECT_CALL(*capabilitiesStub, GetCapabilities(_, _, _))
        .WillRepeatedly(Return(grpc::Status::CANCELLED));

    casClient.setUpFromServerCapabilities();

    // A default limit is used:
    ASSERT_GT(casClient.maxTotalBatchSizeBytes(), 0);
}
