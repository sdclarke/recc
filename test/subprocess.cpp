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

#include <subprocess.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace std;

TEST(SubprocessTest, True)
{
    vector<string> command = {"true"};
    auto result = execute(command);
    EXPECT_EQ(result.d_exitCode, 0);
}

TEST(SubprocessTest, False)
{
    vector<string> command = {"false"};
    auto result = execute(command);
    EXPECT_TRUE(result.d_exitCode != 0);
}

TEST(SubprocessTest, OutputPipes)
{
    vector<string> command = {"echo", "hello", "world"};
    auto result = execute(command, true, true);
    EXPECT_EQ(result.d_exitCode, 0);
    EXPECT_EQ(result.d_stdOut, "hello world\n");
    EXPECT_EQ(result.d_stdErr, "");
}

TEST(SubprocessTest, Environment)
{
    vector<string> command = {"env"};
    map<string, string> env = {{"RECC_SUBPROCESS_TEST_VAR", "value123456"}};
    auto result = execute(command, true, true, env);
    EXPECT_TRUE(result.d_stdOut.find("RECC_SUBPROCESS_TEST_VAR=value123456") !=
                string::npos);
    EXPECT_EQ(result.d_exitCode, 0);
}

TEST(SubprocessTest, WorkingDirectory)
{
    vector<string> command = {"pwd"};
    map<string, string> env;
    auto result = execute(command, true, false, env, "/usr/");
    EXPECT_EQ(result.d_exitCode, 0);
    EXPECT_EQ(result.d_stdOut, "/usr\n");
}
