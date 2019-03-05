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
    const std::string currDir = get_current_working_directory();
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

TEST_F(AuthSessionFiles, ProperRefreshResponse)
{
    // Libcurl accepts file protocol
    // Will make a post request to file and return file content as response
    // Standin for simple mock server. prevents extra dependencies.
    RECC_AUTH_REFRESH_URL = "file://localhost/" + s_serverFilePath;
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    MockPost mockPostFactory;
    EXPECT_CALL(mockPostFactory, generate_post("old_fake"))
        .Times(1)
        .WillOnce(Return("refresh_token=mock_token"));

    AuthSession testSession(&mockPostFactory);
    EXPECT_NO_THROW(testSession.refresh_current_token());
    const std::string response = s_clientFile->read();
    const std::string expectedResponse = s_serverFile->read();
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(AuthSessionFiles, NoRefreshResponse)
{
    const std::string noRefresh = "{\"access_token\": \"fake\"}";
    s_serverFile->write(noRefresh);
    // Libcurl accepts file protocol
    // Will make a post request to file and return file content as response
    // Standin for simple mock server. prevents extra dependencies.
    RECC_AUTH_REFRESH_URL = "file://localhost/" + s_serverFilePath;
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;

    AuthSession testSession(&formPostFactory);
    ASSERT_THROW(testSession.refresh_current_token(), std::runtime_error);

    try {
        testSession.refresh_current_token();
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = JwtErrorUtil::error_to_string(
            "From Auth Server", JwtError::MissingRefreshTokenField);
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST_F(AuthSessionFiles, BadRefreshResponse)
{
    const std::string notJson = "Not Json!";
    s_serverFile->write(notJson);
    // Libcurl accepts file protocol
    // Will make a post request to file and return file content as response
    // Standin for simple mock server. prevents extra dependencies.
    RECC_AUTH_REFRESH_URL = "file://localhost/" + s_serverFilePath;
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;

    AuthSession testSession(&formPostFactory);
    ASSERT_THROW(testSession.refresh_current_token(), std::runtime_error);

    try {
        testSession.refresh_current_token();
    }
    catch (const std::runtime_error &e) {
        std::string error_msg(e.what());
        std::string expected_error = JwtErrorUtil::error_to_string(
            "From Auth Server", JwtError::BadFormat);
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST_F(AuthSessionFiles, BadRefreshUrl)
{
    RECC_AUTH_REFRESH_URL = "https://this-fake-site145365345.fake12345";
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;
    AuthSession testSession(&formPostFactory);

    ASSERT_THROW(testSession.refresh_current_token(), std::runtime_error);
}

TEST_F(AuthSessionFiles, HTTPRefreshUrl)
{
    RECC_AUTH_REFRESH_URL = "http://this-fake-site145365345.fake12345";
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;
    AuthSession testSession(&formPostFactory);

    ASSERT_THROW(testSession.refresh_current_token(), std::runtime_error);
    try {
        testSession.refresh_current_token();
    }
    catch (const std::runtime_error &e) {
        const std::string error_msg(e.what());
        const std::string expected_error =
            "curl_easy_perform() failed: Unsupported protocol";
        EXPECT_EQ(error_msg, expected_error);
    }
}

TEST_F(AuthSessionFiles, RefreshNoURL)
{
    RECC_AUTH_REFRESH_URL = "";
    RECC_JWT_JSON_FILE_PATH = s_clientFilePath;
    FormPost formPostFactory;
    AuthSession testSession(&formPostFactory);

    const std::string oldContents = s_clientFile->read();
    struct stat resultStart;
    int startTime = 0;
    int endTime = 0;
    if (stat(s_clientFilePath.c_str(), &resultStart) == 0) {
        startTime = resultStart.st_atime;
    }
    // sleep for 1 second so access time is different
    usleep(1000000);
    ASSERT_NO_THROW(testSession.refresh_current_token());

    const std::string newContents = s_clientFile->read();
    struct stat resultEnd;
    if (stat(s_clientFilePath.c_str(), &resultEnd) == 0) {
        endTime = resultEnd.st_atime;
    }
    // Make sure file was read
    EXPECT_GT(endTime, startTime);
    // Make sure file wasn't updated
    EXPECT_EQ(newContents, oldContents);
}
