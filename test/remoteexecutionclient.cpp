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

#include <fileutils.h>
#include <merklize.h>
#include <remoteexecutionclient.h>

#include <build/bazel/remote/execution/v2/remote_execution_mock.grpc.pb.h>
#include <gmock/gmock.h>
#include <google/bytestream/bytestream_mock.grpc.pb.h>
#include <google/longrunning/operations_mock.grpc.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
using namespace BloombergLP::recc;
using namespace std;
using namespace testing;

const string emptyString;

#define WITH_CLIENT()                                                         \
    auto executionStub = new proto::MockExecutionStub();                      \
    auto casStub = new proto::MockContentAddressableStorageStub();            \
    auto operationsStub = new google::longrunning::MockOperationsStub();      \
    auto byteStreamStub = new google::bytestream::MockByteStreamStub;         \
    RemoteExecutionClient client(executionStub, casStub, operationsStub,      \
                                 byteStreamStub, string())

MATCHER_P(MessageEq, expected, "")
{
    return google::protobuf::util::MessageDifferencer::Equals(expected, arg);
}

TEST(RemoteExecutionClientTest, ExecuteActionTest)
{
    WITH_CLIENT();

    // Construct the Digest we're passing in, and the ExecuteRequest we expect
    // the RemoteExecutionClient to send as a result.
    proto::Digest actionDigest;
    actionDigest.set_hash("Action digest hash here");
    proto::ExecuteRequest expectedExecuteRequest;
    *expectedExecuteRequest.mutable_action_digest() = actionDigest;

    // Begin contructing a fake ExecuteResponse to return to the client.
    proto::ExecuteResponse executeResponse;
    auto actionResultProto = executeResponse.mutable_result();
    actionResultProto->set_stdout_raw("Raw stdout.");
    string stdErr("Stderr, which will be sent as a digest.");
    auto stdErrDigest = make_digest(stdErr);
    *actionResultProto->mutable_stderr_digest() = stdErrDigest;
    actionResultProto->set_exit_code(123);

    // Add an output file to the response.
    proto::OutputFile outputFile;
    outputFile.set_path("some/path/with/slashes.txt");
    outputFile.mutable_digest()->set_hash("File hash goes here");
    outputFile.mutable_digest()->set_size_bytes(1);
    *actionResultProto->add_output_files() = outputFile;

    // Add an output tree (with nested subdirectories) to the response.
    proto::Tree tree;
    auto treeRootFile = tree.mutable_root()->add_files();
    treeRootFile->mutable_digest()->set_hash("File hash goes here");
    treeRootFile->mutable_digest()->set_size_bytes(1);
    treeRootFile->set_name("out.txt");
    auto childDirectory = tree.add_children();
    auto childDirectoryFile = childDirectory->add_files();
    childDirectoryFile->set_name("a.out");
    childDirectoryFile->mutable_digest()->set_hash("Executable file hash");
    childDirectoryFile->mutable_digest()->set_size_bytes(1);
    childDirectoryFile->set_is_executable(true);
    auto nestedChildDirectory = tree.add_children();
    auto nestedChildDirectoryFile = nestedChildDirectory->add_files();
    nestedChildDirectoryFile->set_name("q.mk");
    nestedChildDirectoryFile->mutable_digest()->set_hash("q.mk file hash");
    nestedChildDirectoryFile->mutable_digest()->set_size_bytes(1);
    *childDirectory->add_directories()->mutable_digest() =
        make_digest(*nestedChildDirectory);
    childDirectory->mutable_directories(0)->set_name("nested");
    *tree.mutable_root()->add_directories()->mutable_digest() =
        make_digest(*childDirectory);
    tree.mutable_root()->mutable_directories(0)->set_name("subdirectory");
    auto treeDigest = make_digest(tree);
    *actionResultProto->add_output_directories()->mutable_tree_digest() =
        treeDigest;
    actionResultProto->mutable_output_directories(0)->set_path(
        "output/directory");

    // Return a completed Operation when the client sends the Execute request.
    google::longrunning::Operation operation;
    operation.set_done(true);
    operation.mutable_response()->PackFrom(executeResponse);
    auto operationReader =
        new grpc::testing::MockClientReader<google::longrunning::Operation>();
    EXPECT_CALL(*executionStub,
                ExecuteRaw(_, MessageEq(expectedExecuteRequest)))
        .WillOnce(Return(operationReader));
    EXPECT_CALL(*operationReader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(operation), Return(true)));
    EXPECT_CALL(*operationReader, Finish()).WillOnce(Return(grpc::Status::OK));

    // Allow the client to fetch the output tree from CAS.
    google::bytestream::ReadRequest expectedByteStreamRequest;
    expectedByteStreamRequest.set_resource_name(
        "blobs/" + treeDigest.hash() + "/" +
        std::to_string(treeDigest.size_bytes()));
    auto reader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();
    google::bytestream::ReadResponse readResponse;
    readResponse.set_data(tree.SerializeAsString());
    EXPECT_CALL(*byteStreamStub,
                ReadRaw(_, MessageEq(expectedByteStreamRequest)))
        .WillOnce(Return(reader));
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(readResponse), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    // Ask the client to execute the action, and make sure the result is
    // correct.
    auto actionResult = client.execute_action(actionDigest);
    EXPECT_EQ(actionResult.exitCode, 123);
    EXPECT_TRUE(actionResult.stdOut.inlined);
    EXPECT_FALSE(actionResult.stdErr.inlined);
    EXPECT_EQ(actionResult.stdOut.blob, "Raw stdout.");
    EXPECT_EQ(actionResult.stdErr.digest.hash(), stdErrDigest.hash());

    EXPECT_EQ(
        actionResult.outputFiles["some/path/with/slashes.txt"].digest.hash(),
        "File hash goes here");
    EXPECT_EQ(
        actionResult.outputFiles["output/directory/out.txt"].digest.hash(),
        "File hash goes here");
    EXPECT_TRUE(actionResult.outputFiles["output/directory/subdirectory/a.out"]
                    .executable);
    EXPECT_EQ(actionResult.outputFiles["output/directory/subdirectory/a.out"]
                  .digest.hash(),
              "Executable file hash");
    EXPECT_EQ(
        actionResult.outputFiles["output/directory/subdirectory/nested/q.mk"]
            .digest.hash(),
        "q.mk file hash");
}

TEST(RemoteExecutionClientTest, WriteFilesToDisk)
{
    WITH_CLIENT();
    TemporaryDirectory tempDir;

    ActionResult testResult;
    File testFile;
    testFile.executable = true;
    testFile.digest.set_hash("Test file hash");
    testFile.digest.set_size_bytes(18);
    testResult.outputFiles["test.txt"] = testFile;

    // Allow the client to fetch the file from CAS.
    google::bytestream::ReadRequest expectedByteStreamRequest;
    expectedByteStreamRequest.set_resource_name(
        "blobs/" + testFile.digest.hash() + "/" +
        std::to_string(testFile.digest.size_bytes()));
    auto reader = new grpc::testing::MockClientReader<
        google::bytestream::ReadResponse>();
    google::bytestream::ReadResponse readResponse;
    readResponse.set_data("Test file content!");
    EXPECT_CALL(*byteStreamStub,
                ReadRaw(_, MessageEq(expectedByteStreamRequest)))
        .WillOnce(Return(reader));
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(readResponse), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    client.write_files_to_disk(testResult, tempDir.name());

    string expectedPath = string(tempDir.name()) + "/test.txt";
    EXPECT_TRUE(is_executable(expectedPath.c_str()));
    EXPECT_EQ(get_file_contents(expectedPath.c_str()), "Test file content!");
}

TEST(RemoteExecutionClientTest, CancelOperation)
{
    WITH_CLIENT();

    // Construct the Digest we're passing in, and the ExecuteRequest we expect
    // the RemoteExecutionClient to send as a result.
    proto::Digest actionDigest;
    actionDigest.set_hash("Action digest hash here");
    proto::ExecuteRequest expectedExecuteRequest;
    *expectedExecuteRequest.mutable_action_digest() = actionDigest;

    // Return a completed Operation when the client sends the Execute request.
    google::longrunning::Operation operation;
    operation.set_done(true);
    auto operationReader =
        new grpc::testing::MockClientReader<google::longrunning::Operation>();
    EXPECT_CALL(*executionStub,
                ExecuteRaw(_, MessageEq(expectedExecuteRequest)))
        .WillOnce(Return(operationReader));
    EXPECT_CALL(*operationReader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(operation), Return(true)));

    // We will be cancelling the request, so expect a call to CancelOperation
    EXPECT_CALL(*operationsStub, CancelOperation(_,_,_)).WillOnce(Return(grpc::Status::OK));

    RemoteExecutionClient::set_cancelled_flag(SIGINT);

    client.execute_action(actionDigest);
}
