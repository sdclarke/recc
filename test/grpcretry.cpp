#include <authsession.h>
#include <authsession_fixture.h>
#include <env.h>
//#include <fileutils.h>
#include <formpost.h>
#include <grpccontext.h>
#include <grpcretry.h>

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <stdlib.h>

using BloombergLP::recc::AuthSession;
using BloombergLP::recc::FormPost;
using BloombergLP::recc::grpc_retry;
using BloombergLP::recc::GrpcContext;
using BloombergLP::recc::JsonFileManager;
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
