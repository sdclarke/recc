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
#include <merklize.h>

#include <build/bazel/remote/execution/v2/remote_execution_mock.grpc.pb.h>
#include <gmock/gmock.h>
#include <google/bytestream/bytestream_mock.grpc.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <regex>

using namespace BloombergLP::recc;
using namespace std;
using namespace testing;

const string abc("abc");
const string defg("defg");
const string emptyString;
const unordered_map<proto::Digest, string> emptyMap;

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

    unordered_map<proto::Digest, string> blobs;
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
    string path = tmpdir.name() + string("/abc.txt");
    write_file(path.c_str(), "abc");

    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    unordered_map<proto::Digest, string> filenames;
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

    unordered_map<proto::Digest, string> blobs;
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
    string path = tmpdir.name() + string("/abc.txt");
    write_file(path.c_str(), "abc");

    auto stub = new proto::MockContentAddressableStorageStub();
    auto byteStreamStub = new google::bytestream::MockByteStreamStub();
    CASClient cas(stub, byteStreamStub, emptyString);

    unordered_map<proto::Digest, string> filenames;
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

    unordered_map<proto::Digest, string> blobs;
    string bigBlob(50000000, 'q');
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

    string storedBlob;
    string name;
    bool isComplete;
    EXPECT_CALL(*writer, Write(_, _))
        .WillRepeatedly(
            DoAll(AddWriteRequestData(&storedBlob, &name, &isComplete),
                  Return(true)));
    EXPECT_CALL(*writer, WritesDone()).WillOnce(Return(true));
    EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

    cas.upload_resources(blobs, emptyMap);

    EXPECT_EQ(storedBlob, bigBlob);

    string uploadNameRegex("uploads\\/"
                           "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{"
                           "4}-[0-9a-f]{12}\\/blobs\\/");
    uploadNameRegex += bigBlobDigest.hash();
    uploadNameRegex += "\\/";
    uploadNameRegex += to_string(bigBlobDigest.size_bytes());
    EXPECT_TRUE(regex_match(name, regex(uploadNameRegex)));
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
