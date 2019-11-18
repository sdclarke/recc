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

#include <authsession.h>
#include <authsession_fixture.h>
#include <env.h>
#include <fileutils.h>
#include <formpost.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

using namespace BloombergLP::recc;
using namespace testing;

class MockPost : public Post {
  public:
    MOCK_METHOD1(generate_post, std::string(std::string refresh_token));
};

TEST_F(AuthSessionFiles, GetProperJwtToken)
{
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;

    FormPost formPostFactory;
    AuthSession testSession(&formPostFactory);

    std::string jwtToken = testSession.get_access_token();
    std::string expectedToken = "old_fake";
    EXPECT_EQ(expectedToken, jwtToken);
}

TEST_F(AuthSessionFiles, GetImproperJwtToken)
{
    const std::string improper = "{\"refresh_token\": \"fake\"}";
    s_clientFile->write(improper);
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;

    ASSERT_THROW(AuthSession testSession(&formPostFactory),
                 std::runtime_error);
    try {
        AuthSession testSessionAgain(&formPostFactory);
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = JwtErrorUtil::error_to_string(
            s_clientFilePath, JwtError::MissingAccessTokenField);
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST_F(AuthSessionFiles, GetNotJwtToken)
{
    const std::string notJson = "Not Json!";
    s_clientFile->write(notJson);
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;

    ASSERT_THROW(AuthSession testSession(&formPostFactory),
                 std::runtime_error);
    try {
        AuthSession testSessionAgain(&formPostFactory);
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = JwtErrorUtil::error_to_string(
            s_clientFilePath, JwtError::BadFormat);
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST_F(AuthSessionFiles, GetNotExist)
{
    const std::string currDir = FileUtils::get_current_working_directory();
    const std::string fakePath = currDir + "/" + "fake_file.fake";

    RECC_JWT_JSON_FILE_PATH = fakePath;
    FormPost formPostFactory;
    ASSERT_THROW(AuthSession testSession(&formPostFactory),
                 std::runtime_error);
    try {
        AuthSession testSessionAgain(&formPostFactory);
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error =
            JwtErrorUtil::error_to_string(fakePath, JwtError::NotExist);
        EXPECT_EQ(error_msg, expected_error);
    }
}
