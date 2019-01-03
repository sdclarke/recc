#include <env.h>
#include <grpcretry.h>

#include <gtest/gtest.h>

#include <iostream>

using BloombergLP::recc::grpc_retry;
using BloombergLP::recc::RECC_RETRY_LIMIT;

TEST(GrpcRetry, SimpleRetrySucceedTest)
{
    RECC_RETRY_LIMIT = 1;

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

    EXPECT_NO_THROW(grpc_retry(lambda));
}

TEST(GrpcRetry, SimpleRetryFailTest)
{
    RECC_RETRY_LIMIT = 2;
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

    EXPECT_THROW(grpc_retry(lambda), std::runtime_error);
}
