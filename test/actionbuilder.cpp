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

#include <actionbuilder.h>
#include <env.h>
#include <fileutils.h>
#include <merklize.h>
#include <protos.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;

class ActionBuilderTestFixture : public ::testing::Test {
  protected:
    std::unordered_map<proto::Digest, std::string> blobs;
    std::unordered_map<proto::Digest, std::string> filenames;
    std::string cwd;
    ActionBuilderTestFixture() : cwd(get_current_working_directory()) {}
};

void fail_to_build_action(const std::vector<std::string> &recc_args)
{
    std::stringstream ss;
    for (const auto &arg : recc_args) {
        ss << arg << " ";
    }
    FAIL() << "Test was unable to build action for '" << ss.str() << "'";
}

/**
 * Builds an action for test/data/actionbuilder/hello.cpp and checks against
 * precomputed digest values
 */
TEST_F(ActionBuilderTestFixture, ActionBuilt)
{
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());

    auto actionPtr =
        ActionBuilder::BuildAction(command, cwd, &blobs, &filenames);

    if (!actionPtr) {
        fail_to_build_action(recc_args);
    }

    EXPECT_EQ(
        "37db471e0c1faf092b9026586891cde0daefbdd8e5a05853360e7539179c8371",
        actionPtr->command_digest().hash());

    EXPECT_EQ(
        "05a73e8a93752c197cb58e4bdb43bd1d696fa02ecff899a339c4ecacbab2c14b",
        actionPtr->input_root_digest().hash());

    auto digest = make_digest(*actionPtr);
    EXPECT_EQ(140, digest.size_bytes());
    EXPECT_EQ(
        "4137c380ea4c2ccf8aba9a252926bdb884d16185506e644e1df0e8a76dbd3d94",
        digest.hash());
}

/**
 * Non-compile commands return nullptrs, indicating that they are intended to
 * be run locally if RECC_FORCE_REMOTE isn't set
 */
TEST_F(ActionBuilderTestFixture, NonCompileCommand)
{
    bool previousReccForceRemote = RECC_FORCE_REMOTE;
    RECC_FORCE_REMOTE = false;
    std::vector<std::string> recc_args = {"ls"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr =
        ActionBuilder::BuildAction(command, cwd, &blobs, &filenames);

    ASSERT_EQ(actionPtr, nullptr);
    RECC_FORCE_REMOTE = previousReccForceRemote;
}

/**
 * However, force remoting a non-compile command still generates an action
 */
TEST_F(ActionBuilderTestFixture, NonCompileCommandForceRemote)
{
    bool previousReccForceRemote = RECC_FORCE_REMOTE;
    RECC_FORCE_REMOTE = true;
    std::vector<std::string> recc_args = {"ls"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr =
        ActionBuilder::BuildAction(command, cwd, &blobs, &filenames);

    if (!actionPtr) {
        fail_to_build_action(recc_args);
    }

    EXPECT_EQ(
        "011cf8e3cb5ebadcb43b4088f1c5e7c76ddb2812ab6e3afc062922d80b937110",
        actionPtr->command_digest().hash());

    EXPECT_EQ(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        actionPtr->input_root_digest().hash());

    auto digest = make_digest(*actionPtr);

    EXPECT_EQ(138, digest.size_bytes());
    EXPECT_EQ(
        "2be362296e706dc8a62dba77b81bd0b951347f733e2f8f706aa5f69041ef8ba7",
        digest.hash());

    RECC_FORCE_REMOTE = previousReccForceRemote;
}

/**
 * Trying to output to an unrelated directory returns a nullptr, forcing a
 * local execution.
 */
TEST_F(ActionBuilderTestFixture, UnrelatedOutputDirectory)
{
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "/fakedirname/hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr =
        ActionBuilder::BuildAction(command, cwd, &blobs, &filenames);

    ASSERT_EQ(actionPtr, nullptr);
}
