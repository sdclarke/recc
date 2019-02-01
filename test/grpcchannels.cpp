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

#include <env.h>
#include <fileutils.h>
#include <grpcchannels.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace testing;

TEST(GrpcChannelsTest, JwtError)
{
    std::string json_path = "fake_path";
    std::ostringstream err_msg;
    err_msg << "JWT authentication token: " << json_path;

    std::string not_exist_err =
        err_msg.str() + " does not exist or can't be read";
    EXPECT_EQ(not_exist_err, GrpcChannels::jwt_error(
                                 json_path, GrpcChannels::jwtError::NotExist));

    std::string not_json_err =
        err_msg.str() + " found but JSON could not be parsed";
    EXPECT_EQ(not_json_err, GrpcChannels::jwt_error(
                                json_path, GrpcChannels::jwtError::BadFormat));

    std::string missing_token_err =
        err_msg.str() + " missing field access_token";
    EXPECT_EQ(missing_token_err,
              GrpcChannels::jwt_error(
                  json_path, GrpcChannels::jwtError::MissingAccessTokenField));
}

TEST(GrpcChannelsTest, GetProperJwtToken)
{
    std::string properJwt =
        get_current_working_directory() + "/" + "proper_jwt";
    std::string jwtToken = std::string(GrpcChannels::get_jwt_token(properJwt));
    std::string expectedToken = "fake_token";
    EXPECT_EQ(expectedToken, jwtToken);
}

TEST(GrpcChannelsTest, GetImproperJwtToken)
{
    std::string improperJwt =
        get_current_working_directory() + "/" + "improper_jwt";
    ASSERT_THROW(GrpcChannels::get_jwt_token(improperJwt), std::runtime_error);
    try {
        GrpcChannels::get_jwt_token(improperJwt);
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = GrpcChannels::jwt_error(
            improperJwt, GrpcChannels::jwtError::MissingAccessTokenField);
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST(GrpcChannelsTest, GetNotJwtToken)
{
    std::string notJson = get_current_working_directory() + "/" + "not_json";
    ASSERT_THROW(GrpcChannels::get_jwt_token(notJson), std::runtime_error);
    try {
        GrpcChannels::get_jwt_token(notJson);
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = GrpcChannels::jwt_error(
            notJson, GrpcChannels::jwtError::BadFormat);
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST(GrpcChannelsTest, GetNotExist)
{
    std::string notExist = get_current_working_directory() + "/" + "not_exist";
    ASSERT_THROW(GrpcChannels::get_jwt_token(notExist), std::runtime_error);
    try {
        GrpcChannels::get_jwt_token(notExist);
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = GrpcChannels::jwt_error(
            notExist, GrpcChannels::jwtError::NotExist);
        EXPECT_EQ(error_msg, expected_error);
    }
}
