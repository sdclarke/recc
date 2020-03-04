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

#include <buildboxcommonmetrics_durationmetrictimer.h>
#include <buildboxcommonmetrics_testingutils.h>
#include <digestgenerator.h>
#include <env.h>
#include <grpccontext.h>
#include <logging.h>
#include <remoteexecutionclient.h>

#include <buildboxcommon_temporarydirectory.h>

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
#include <set>
#include <signal.h>
#include <unistd.h>

#define TIMER_NAME_FETCH_WRITE_RESULTS "recc.fetch_write_results"

using namespace BloombergLP::recc;
using namespace buildboxcommon::buildboxcommonmetrics;
using namespace testing;

/*
 * This fixture sets up all of the dependencies for execute_action
 *
 * Any dependencies can be overridden by setting them in the respective tests
 */
class RemoteExecutionClientTestFixture : public ::testing::Test {
  protected:
    std::shared_ptr<proto::MockExecutionStub> executionStub;
    std::shared_ptr<proto::MockContentAddressableStorageStub> casStub;
    std::shared_ptr<proto::MockCapabilitiesStub> casCapabilitiesStub;
    std::shared_ptr<proto::MockActionCacheStub> actionCacheStub;
    std::shared_ptr<google::longrunning::MockOperationsStub> operationsStub;
    std::shared_ptr<google::bytestream::MockByteStreamStub> byteStreamStub;

    const std::string instance_name = "";

    GrpcContext grpcContext;

    RemoteExecutionClient client;

    proto::Digest actionDigest;
    proto::ExecuteRequest expectedExecuteRequest;

    proto::Digest stdErrDigest;

    google::longrunning::Operation operation;
    grpc::testing::MockClientReader<google::longrunning::Operation>
        *operationReader;

    google::bytestream::ReadRequest expectedByteStreamRequest;

    grpc::testing::MockClientReader<google::bytestream::ReadResponse> *reader;

    google::bytestream::ReadResponse readResponse;

    RemoteExecutionClientTestFixture()
        : executionStub(std::make_shared<proto::MockExecutionStub>()),
          casStub(
              std::make_shared<proto::MockContentAddressableStorageStub>()),
          actionCacheStub(std::make_shared<proto::MockActionCacheStub>()),
          operationsStub(
              std::make_shared<google::longrunning::MockOperationsStub>()),
          byteStreamStub(
              std::make_shared<google::bytestream::MockByteStreamStub>()),
          client(executionStub, casStub, casCapabilitiesStub, actionCacheStub,
                 operationsStub, byteStreamStub, instance_name, &grpcContext)
    {

        // Construct the Digest we're passing in, and the ExecuteRequest we
        // expect the RemoteExecutionClient to send as a result.
        actionDigest.set_hash("Action digest hash here");
        *expectedExecuteRequest.mutable_action_digest() = actionDigest;

        // Begin constructing a fake ExecuteResponse to return to the client.
        proto::ExecuteResponse executeResponse;
        const auto actionResultProto = executeResponse.mutable_result();
        actionResultProto->set_stdout_raw("Raw stdout.");
        std::string stdErr("Stderr, which will be sent as a digest.");
        stdErrDigest = DigestGenerator::make_digest(stdErr);
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
            DigestGenerator::make_digest(*nestedChildDirectory);
        childDirectory->mutable_directories(0)->set_name("nested");
        *tree.mutable_root()->add_directories()->mutable_digest() =
            DigestGenerator::make_digest(*childDirectory);
        tree.mutable_root()->mutable_directories(0)->set_name("subdirectory");
        const auto treeDigest = DigestGenerator::make_digest(tree);
        *actionResultProto->add_output_directories()->mutable_tree_digest() =
            treeDigest;
        actionResultProto->mutable_output_directories(0)->set_path(
            "output/directory");

        // Return a completed Operation when the client sends the Execute
        // request.
        operation.set_done(true);
        operation.mutable_response()->PackFrom(executeResponse);
        operationReader = new grpc::testing::MockClientReader<
            google::longrunning::Operation>();

        reader = new grpc::testing::MockClientReader<
            google::bytestream::ReadResponse>();

        // Allow the client to fetch the output tree from CAS.
        expectedByteStreamRequest.set_resource_name(
            "blobs/" + treeDigest.hash() + "/" +
            std::to_string(treeDigest.size_bytes()));
        readResponse.set_data(tree.SerializeAsString());
    }

    ~RemoteExecutionClientTestFixture() {}
};

MATCHER_P(MessageEq, expected, "")
{
    return google::protobuf::util::MessageDifferencer::Equals(expected, arg);
}

TEST_F(RemoteExecutionClientTestFixture, ExecuteActionTest)
{
    // Set up the behavior for the various mock dependencies
    EXPECT_CALL(*executionStub,
                ExecuteRaw(_, MessageEq(expectedExecuteRequest)))
        .WillOnce(Return(operationReader));
    EXPECT_CALL(*operationReader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(operation), Return(true)));
    EXPECT_CALL(*operationReader, Finish()).WillOnce(Return(grpc::Status::OK));

    EXPECT_CALL(*byteStreamStub,
                ReadRaw(_, MessageEq(expectedByteStreamRequest)))
        .WillOnce(Return(reader));
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(readResponse), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    // Ask the client to execute the action, and make sure the result is
    // correct.
    const auto actionResult = client.execute_action(actionDigest);

    EXPECT_EQ(actionResult.d_exitCode, 123);
    EXPECT_TRUE(actionResult.d_stdOut.d_inlined);
    EXPECT_FALSE(actionResult.d_stdErr.d_inlined);
    EXPECT_EQ(actionResult.d_stdOut.d_blob, "Raw stdout.");
    EXPECT_EQ(actionResult.d_stdErr.d_digest.hash(), stdErrDigest.hash());

    EXPECT_EQ(actionResult.d_outputFiles.at("some/path/with/slashes.txt")
                  .d_digest.hash(),
              "File hash goes here");
    EXPECT_EQ(actionResult.d_outputFiles.at("output/directory/out.txt")
                  .d_digest.hash(),
              "File hash goes here");
    EXPECT_TRUE(
        actionResult.d_outputFiles.at("output/directory/subdirectory/a.out")
            .d_executable);
    EXPECT_EQ(
        actionResult.d_outputFiles.at("output/directory/subdirectory/a.out")
            .d_digest.hash(),
        "Executable file hash");
    EXPECT_EQ(actionResult.d_outputFiles
                  .at("output/directory/subdirectory/nested/q.mk")
                  .d_digest.hash(),
              "q.mk file hash");
}

TEST_F(RemoteExecutionClientTestFixture, RpcRetryTest)
{
    int old_retry_limit = RECC_RETRY_LIMIT;
    RECC_RETRY_LIMIT = 2;

    grpc::testing::MockClientReader<
        google::longrunning::Operation> *brokenOperationReader =
        new grpc::testing::MockClientReader<google::longrunning::Operation>();

    // Execution stuff
    EXPECT_CALL(*executionStub,
                ExecuteRaw(_, MessageEq(expectedExecuteRequest)))
        .WillOnce(Return(brokenOperationReader))
        .WillOnce(Return(operationReader));

    EXPECT_CALL(*brokenOperationReader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(operation), Return(false)));
    EXPECT_CALL(*brokenOperationReader, Finish())
        .WillOnce(Return(grpc::Status(grpc::FAILED_PRECONDITION, "failed")));

    EXPECT_CALL(*operationReader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(operation), Return(true)));
    EXPECT_CALL(*operationReader, Finish()).WillOnce(Return(grpc::Status::OK));

    // CAS Stuff
    EXPECT_CALL(*byteStreamStub,
                ReadRaw(_, MessageEq(expectedByteStreamRequest)))
        .WillOnce(Return(reader));
    EXPECT_CALL(*reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(readResponse), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

    // Ask the client to execute the action, and make sure the result is
    // correct.
    const auto actionResult = client.execute_action(actionDigest);

    RECC_RETRY_LIMIT = old_retry_limit;
}

TEST_F(RemoteExecutionClientTestFixture, WriteFilesToDisk)
{
    buildboxcommon::TemporaryDirectory tempDir;

    ActionResult testResult;
    proto::Digest d;
    d.set_hash("Test file hash");
    d.set_size_bytes(18);
    auto testFile = OutputBlob(std::string(), d, true);
    testResult.d_outputFiles["test.txt"] = testFile;

    // Allow the client to fetch the file from CAS.
    google::bytestream::ReadRequest expectedByteStreamRequest;
    expectedByteStreamRequest.set_resource_name(
        "blobs/" + testFile.d_digest.hash() + "/" +
        std::to_string(testFile.d_digest.size_bytes()));
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

    const std::string expectedPath = std::string(tempDir.name()) + "/test.txt";
    EXPECT_TRUE(FileUtils::isExecutable(expectedPath));
    EXPECT_EQ(FileUtils::getFileContents(expectedPath), "Test file content!");
}

TEST_F(RemoteExecutionClientTestFixture, VerifyMetricsWriteFilesToDisk)
{
    buildboxcommon::TemporaryDirectory tempDir;

    ActionResult testResult;
    proto::Digest d;
    d.set_hash("Test file hash");
    d.set_size_bytes(18);
    auto testFile = OutputBlob(std::string(), d, true);
    testResult.d_outputFiles["test.txt"] = testFile;

    // Allow the client to fetch the file from CAS.
    google::bytestream::ReadRequest expectedByteStreamRequest;
    expectedByteStreamRequest.set_resource_name(
        "blobs/" + testFile.d_digest.hash() + "/" +
        std::to_string(testFile.d_digest.size_bytes()));
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
    EXPECT_TRUE(validateMetricCollection<DurationMetricTimer>(
        TIMER_NAME_FETCH_WRITE_RESULTS));
}

TEST_F(RemoteExecutionClientTestFixture, CancelOperation)
{
    /**
     * This test is a bit gross, so it warrants an explanation.
     *
     * We're testing the SIGINT handler here, and the cleanest way to do that
     * is to send the signal from another process. Since GTest won't fail
     * loudly in child processes, we'll have the child send the signal to the
     * parent rather than the other way around. Moreover, since sending SIGINT
     * results in a call to exit(), we need to do all of the work inside an
     * ASSERT_EXIT block, which itself forks.
     *
     * execute_action() does, among other things, the following things in
     * order:
     *
     *     1. Sets up a SIGINT signal handler
     *     2. Calls Execute()
     *     3. Calls Read() to get the result
     *
     * This test takes advantage of this order to override the mock Execute()
     * (in the parent) to write its PID to the timing pipe. When this happens,
     * the child knows that the signal handler has been set up in the parent,
     * so it can safely send SIGINT to the assert block. This SIGINT should get
     * picked up by the parent's signal handler, and the busy wait should
     * eventually pick up on the cancellation flag to send the
     * CancelOperation() RPC.
     *
     * If any of the EXPECT_CALLS inside ASSERT_EXIT fail, the process running
     * ASSERT_EXIT returns 1, which fails the assert block.
     *
     * It's unfortunate that this test relies on the implementation detail of
     * the signal handler being set up before Execute is called, but testing
     * signal handlers gets rather messy this way.
     */

    int timingPipe[2];
    if (pipe(timingPipe)) {
        RECC_LOG_ERROR("Failed to create pipe");
        FAIL();
    }

    pid_t pid = fork();

    if (pid == static_cast<pid_t>(0)) {
        close(timingPipe[1]);

        /* Wait for the parent to write its PID before sending the signal */
        pid_t assert_pid = getpid();
        read(timingPipe[0], &assert_pid, sizeof(assert_pid));
        close(timingPipe[0]);
        kill(assert_pid, SIGINT);
    }

    else {
        close(timingPipe[0]);

        ASSERT_EXIT(
            {
                /**
                 * This lambda replaces the behavior of Execute()
                 * It writes its PID to the pipe to indicate to the child that
                 * it has called this RPC. This means that the signal handler
                 * has already been set up in execute_action.
                 */

                // Return an incomplete Operation when the client sends the
                // Execute request.
                operation.set_done(false);
                operation.set_name("fake-operation");

                // We need to declare operationReader locally to capture it
                // in the lambda
                auto operationReader = new grpc::testing::MockClientReader<
                    google::longrunning::Operation>();

                EXPECT_CALL(*executionStub,
                            ExecuteRaw(_, MessageEq(expectedExecuteRequest)))
                    .WillOnce(testing::InvokeWithoutArgs([&timingPipe,
                                                          &operationReader]() {
                        pid_t assert_pid = getpid();
                        write(timingPipe[1], &assert_pid, sizeof(assert_pid));
                        close(timingPipe[1]);
                        return operationReader;
                    }));

                EXPECT_CALL(*operationReader, Read(_))
                    .WillRepeatedly(
                        DoAll(SetArgPointee<0>(operation), Return(true)));
                EXPECT_CALL(*operationsStub, CancelOperation(_, _, _))
                    .WillOnce(Return(grpc::Status::OK));

                /*
                 * Since we call exit(), we leak several mocks and make GTest
                 * unhappy. We need to tell GTest to ignore these leaks during
                 * the memory check.
                 */
                Mock::AllowLeak(executionStub.get());
                Mock::AllowLeak(operationReader);
                Mock::AllowLeak(operationsStub.get());

                client.execute_action(actionDigest);
            },
            ::testing::ExitedWithCode(130), ".*");
    }
    waitpid(pid, nullptr, 0);
}

TEST_F(RemoteExecutionClientTestFixture, ActionCacheTestMiss)
{
    EXPECT_CALL(*actionCacheStub, GetActionResult(_, _, _))
        .WillOnce(Return(grpc::Status(grpc::NOT_FOUND, "not found")));
    // `NOT_FOUND` => return false and do not write the ActionResult parameter.

    ActionResult actionResult;
    actionResult.d_exitCode = 123;

    ActionResult actionResultOut = actionResult;
    std::set<std::string> outputs;

    const bool in_cache = client.fetch_from_action_cache(
        actionDigest, outputs, std::string(), &actionResultOut);

    EXPECT_FALSE(in_cache);
    EXPECT_EQ(actionResultOut.d_exitCode, actionResult.d_exitCode);
}

TEST_F(RemoteExecutionClientTestFixture, ActionCacheTestHit)
{
    EXPECT_CALL(*actionCacheStub, GetActionResult(_, _, _))
        .WillOnce(Return(grpc::Status::OK));
    // Return true and write the ActionResult parameter with the fetched one.

    ActionResult actionResult;
    actionResult.d_exitCode = 123;

    ActionResult actionResultOut = actionResult;
    std::set<std::string> outputs;

    const bool in_cache = client.fetch_from_action_cache(
        actionDigest, outputs, std::string(), &actionResultOut);

    EXPECT_TRUE(in_cache);
    EXPECT_EQ(actionResultOut.d_exitCode, 0);
}

TEST_F(RemoteExecutionClientTestFixture, ActionCacheTestServerError)
{
    EXPECT_CALL(*actionCacheStub, GetActionResult(_, _, _))
        .WillOnce(Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                                      "permission denied")));
    // All other server errors other than `NOT_FOUND` are thrown.

    ActionResult actionResultOut;
    std::set<std::string> outputs;
    ASSERT_THROW(client.fetch_from_action_cache(
                     actionDigest, outputs, std::string(), &actionResultOut),
                 std::runtime_error);
}

TEST_F(RemoteExecutionClientTestFixture, ActionCacheTestMissNoActionResult)
{
    EXPECT_CALL(*actionCacheStub, GetActionResult(_, _, _))
        .WillOnce(Return(grpc::Status(grpc::NOT_FOUND, "not found")));
    // `NOT_FOUND` => return false and do not write the ActionResult parameter.

    std::set<std::string> outputs;
    const bool in_cache = client.fetch_from_action_cache(
        actionDigest, outputs, std::string(), nullptr);

    EXPECT_FALSE(in_cache);
}

TEST_F(RemoteExecutionClientTestFixture, ActionCacheHitNoActionResult)
{
    EXPECT_CALL(*actionCacheStub, GetActionResult(_, _, _))
        .WillOnce(Return(grpc::Status::OK));

    std::set<std::string> outputs;

    const bool in_cache = client.fetch_from_action_cache(
        actionDigest, outputs, std::string(), nullptr);

    EXPECT_TRUE(in_cache);
}
