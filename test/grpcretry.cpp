#include <authsession.h>
#include <authsession_fixture.h>
#include <env.h>
#include <fileutils.h>
#include <formpost.h>
#include <grpccontext.h>
#include <grpcretry.h>

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <stdlib.h>

using BloombergLP::recc::AuthSession;
using BloombergLP::recc::FormPost;
using BloombergLP::recc::get_current_working_directory;
using BloombergLP::recc::grpc_retry;
using BloombergLP::recc::GrpcContext;
using BloombergLP::recc::JsonFileManager;
using BloombergLP::recc::RECC_AUTH_REFRESH_URL;
using BloombergLP::recc::RECC_JWT_JSON_FILE_PATH;
using BloombergLP::recc::RECC_RETRY_LIMIT;

TEST(GrpcRetry, SimpleRetrySucceedTest)
{
    RECC_RETRY_LIMIT = 1;
    GrpcContext grpcContext;
    int failures = 0;

    /* Fail once, then succeed. */
    auto lambda = [&](grpc::ClientContext &context) {
        if (failures < 1) {
            failures++;
            return grpc::Status(grpc::FAILED_PRECONDITION, "failing in test");
        }
        else {
            return grpc::Status::OK;
        }
    };

    EXPECT_NO_THROW(grpc_retry(lambda, &grpcContext));
}

TEST(GrpcRetry, SimpleRetryFailTest)
{
    RECC_RETRY_LIMIT = 2;
    GrpcContext grpcContext;
    int failures = 0;
    /* Fail three times, then succeed. */
    auto lambda = [&](grpc::ClientContext &context) {
        if (failures < 3) {
            failures++;
            return grpc::Status(grpc::FAILED_PRECONDITION, "failing in test");
        }
        else {
            return grpc::Status::OK;
        }
    };

    EXPECT_THROW(grpc_retry(lambda, &grpcContext), std::runtime_error);
}

// AuthSessionFiles is a fixture which will reset test files
// after each test. So the individual tests don't have to worry about it.
TEST_F(AuthSessionFiles, CheckRefresh)
{

    // using file to simulate server response
    // curl will try posting to file and treat file contents as response
    RECC_AUTH_REFRESH_URL = "file://localhost/" + s_serverFilePath;
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;
    std::unique_ptr<AuthSession> reccAuthSession(
        new AuthSession(&formPostFactory));
    GrpcContext grpcContext(reccAuthSession.get());

    // purposely keep LIMIT low to test if
    // refresh is not using a retry the first time
    RECC_RETRY_LIMIT = 1;
    int failures = 0;

    /* Refresh once, then succeed. */
    auto lambda = [&](grpc::ClientContext &context) {
        if (failures == 0) {
            failures++;
            return grpc::Status(grpc::FAILED_PRECONDITION, "failing in test");
        }
        else if (failures == 1) {
            failures++;
            return grpc::Status(grpc::UNAUTHENTICATED,
                                "Unauthenticated in test");
        }
        else {
            return grpc::Status::OK;
        }
    };

    EXPECT_NO_THROW(grpc_retry(lambda, &grpcContext));

    std::string response = s_clientFile->read();
    std::string expectedResponse = s_serverFile->read();

    EXPECT_EQ(response, expectedResponse);
}

// checking if GrpcRetry will only refresh once
TEST_F(AuthSessionFiles, CheckTwoAuth)
{

    // using file to simulate server response
    // curl will try posting to file and treat file contents as response
    RECC_AUTH_REFRESH_URL = "file://localhost/" + s_serverFilePath;
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;
    std::unique_ptr<AuthSession> reccAuthSession(
        new AuthSession(&formPostFactory));
    GrpcContext grpcContext(reccAuthSession.get());

    RECC_RETRY_LIMIT = 0;
    int failures = 0;

    /* Refresh once, should not refresh again, fail. */
    auto lambda = [&](grpc::ClientContext &context) {
        if (failures <= 1) {
            failures++;
            return grpc::Status(grpc::UNAUTHENTICATED, "failing in test");
        }
        else {
            return grpc::Status::OK;
        }
    };

    EXPECT_THROW(grpc_retry(lambda, &grpcContext), std::runtime_error);

    std::string response = s_clientFile->read();
    std::string expectedResponse = s_serverFile->read();

    // check if refreshed once at least
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(AuthSessionFiles, UrlNotSet)
{
    RECC_AUTH_REFRESH_URL = "";
    FormPost formPostFactory;
    std::unique_ptr<AuthSession> reccAuthSession(
        new AuthSession(&formPostFactory));
    GrpcContext grpcContext(reccAuthSession.get());

    std::string oldToken = s_clientFile->read();

    RECC_RETRY_LIMIT = 0;
    int failures = 0;
    /* Fail three times, then succeed. */
    auto lambda = [&](grpc::ClientContext &context) {
        if (failures <= 0) {
            failures++;
            return grpc::Status(grpc::UNAUTHENTICATED, "failing in test");
        }
        else {
            return grpc::Status::OK;
        }
    };

    EXPECT_NO_THROW(grpc_retry(lambda, &grpcContext));

    std::string newToken = s_clientFile->read();
    std::string expectedResponse = s_serverFile->read();

    EXPECT_EQ(oldToken, newToken);
    EXPECT_NE(newToken, expectedResponse);
}
