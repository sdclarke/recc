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
#include <digestgenerator.h>
#include <env.h>
#include <fileutils.h>
#include <protos.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;

class ActionBuilderTestFixture : public ::testing::Test {
  protected:
    ActionBuilderTestFixture() : cwd(FileUtils::getCurrentWorkingDirectory())
    {
    }

    void SetUp() override
    {
        d_previous_recc_replacement_map = RECC_PREFIX_REPLACEMENT;
        d_previous_deps_override = RECC_DEPS_OVERRIDE;
        d_previous_force_remote = RECC_FORCE_REMOTE;
        d_previous_deps_global_path = RECC_DEPS_GLOBAL_PATHS;
        d_previous_deps_exclude_paths = RECC_DEPS_EXCLUDE_PATHS;
    }

    void TearDown() override
    {
        RECC_PREFIX_REPLACEMENT = d_previous_recc_replacement_map;
        RECC_DEPS_OVERRIDE = d_previous_deps_override;
        RECC_FORCE_REMOTE = d_previous_force_remote;
        RECC_DEPS_GLOBAL_PATHS = d_previous_deps_global_path;
        RECC_DEPS_EXCLUDE_PATHS = d_previous_deps_exclude_paths;
    }

    digest_string_umap blobs;
    digest_string_umap digest_to_filecontents;
    std::string cwd;

  private:
    std::vector<std::pair<std::string, std::string>>
        d_previous_recc_replacement_map;
    std::set<std::string> d_previous_deps_override;
    std::set<std::string> d_previous_deps_exclude_paths;
    bool d_previous_force_remote;
    bool d_previous_deps_global_path;
};

// This is a representation of a flat merkle tree, where things are stored in
// the required order of traversal.
typedef std::vector<std::unordered_map<std::string, std::vector<std::string>>>
    MerkleTree;
typedef std::vector<
    std::unordered_map<std::string, std::vector<std::string>>>::iterator
    MerkleTreeItr;

void fail_to_build_action(const std::vector<std::string> &recc_args)
{
    std::stringstream ss;
    for (const auto &arg : recc_args) {
        ss << arg << " ";
    }
    FAIL() << "Test was unable to build action for '" << ss.str() << "'";
}

// Recursively verify that a merkle tree matches an expected input layout.
// This doesn't look at the hashes, just that the declared layout matches
void verify_merkle_tree(proto::Digest digest, MerkleTreeItr expected,
                        MerkleTreeItr end, digest_string_umap blobs)
{
    ASSERT_NE(expected, end) << "Reached end of expected output early";
    proto::Directory directory;
    auto current_blob = blobs.find(digest);
    ASSERT_NE(current_blob, blobs.end())
        << "No blob found for digest " << digest.hash();

    directory.ParseFromString(current_blob->second);

    // Exit early if there are more/less files or dirs in the given tree than
    // expected
    ASSERT_EQ(directory.files().size(), (*expected)["files"].size())
        << "Wrong number of files at current level";
    ASSERT_EQ(directory.directories().size(),
              (*expected)["directories"].size())
        << "Wrong number of directories at current level";

    int f_index = 0;
    for (auto &file : directory.files()) {
        ASSERT_EQ(file.name(), (*expected)["files"][f_index])
            << "Wrong file found";
        f_index++;
    }
    int d_index = 0;
    for (auto &subdirectory : directory.directories()) {
        ASSERT_EQ(subdirectory.name(), (*expected)["directories"][d_index])
            << "Wrong directory found";
        d_index++;
    }
    // All the files/directories at this level are correct, now check all the
    // subdirectories
    for (auto &subdirectory : directory.directories()) {
        verify_merkle_tree(subdirectory.digest(), ++expected, end, blobs);
    }
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

    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    if (!actionPtr) {
        fail_to_build_action(recc_args);
    }

    EXPECT_EQ(
        "37db471e0c1faf092b9026586891cde0daefbdd8e5a05853360e7539179c8371",
        actionPtr->command_digest().hash());

    EXPECT_EQ(
        "05a73e8a93752c197cb58e4bdb43bd1d696fa02ecff899a339c4ecacbab2c14b",
        actionPtr->input_root_digest().hash());

    auto digest = DigestGenerator::make_digest(*actionPtr);
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
    RECC_FORCE_REMOTE = false;
    std::vector<std::string> recc_args = {"ls"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    ASSERT_EQ(actionPtr, nullptr);
}

/**
 * However, force remoting a non-compile command still generates an action
 */
TEST_F(ActionBuilderTestFixture, NonCompileCommandForceRemote)
{
    RECC_FORCE_REMOTE = true;
    std::vector<std::string> recc_args = {"ls"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    if (!actionPtr) {
        fail_to_build_action(recc_args);
    }

    EXPECT_EQ(
        "011cf8e3cb5ebadcb43b4088f1c5e7c76ddb2812ab6e3afc062922d80b937110",
        actionPtr->command_digest().hash());

    EXPECT_EQ(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        actionPtr->input_root_digest().hash());

    auto digest = DigestGenerator::make_digest(*actionPtr);

    EXPECT_EQ(138, digest.size_bytes());
    EXPECT_EQ(
        "2be362296e706dc8a62dba77b81bd0b951347f733e2f8f706aa5f69041ef8ba7",
        digest.hash());
}

/**
 * Make sure an absolute file dependencies are represented properly. Do this by
 * using RECC_DEPS_OVERRIDE to return a deterministic set of dependencies.
 * Otherwise things are platform dependant
 */
TEST_F(ActionBuilderTestFixture, AbsolutePathActionBuilt)
{
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    // Since this test unfortunately relies on the hash of a system installed
    // file, it can't compare the input root digest. Instead go through the
    // merkle tree and verify each level has the correct layout.

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {
        {{"files", {"hello.cpp"}}, {"directories", {"usr"}}},
        {{"directories", {"include"}}},
        {{"files", {"ctype.h"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}

/**
 * Test to make sure that paths with .. are properly resolved using
 * information from the current directory, and that it doesn't cause
 * issues with absolute paths
 */
TEST_F(ActionBuilderTestFixture, RelativePathAndAbsolutePathWithCwd)
{
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {
        {{"directories", {"actionbuilder", "deps", "usr"}}},
        {{"files", {"hello.cpp"}}},
        {{"files", {"empty.c"}}},
        {{"directories", {"include"}}},
        {{"files", {"ctype.h"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}

TEST_F(ActionBuilderTestFixture, ExcludePath)
{
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/usr/include"};
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                                {{"files", {"hello.cpp"}}},
                                {{"files", {"empty.c"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}

TEST_F(ActionBuilderTestFixture, ExcludePathMultipleMatchOne)
{
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/foo/bar", "/usr/include"};
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                                {{"files", {"hello.cpp"}}},
                                {{"files", {"empty.c"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}

TEST_F(ActionBuilderTestFixture, ExcludePathNoMatch)
{
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/foo/bar"};
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {
        {{"directories", {"actionbuilder", "deps", "usr"}}},
        {{"files", {"hello.cpp"}}},
        {{"files", {"empty.c"}}},
        {{"directories", {"include"}}},
        {{"files", {"ctype.h"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}

TEST_F(ActionBuilderTestFixture, ExcludePathSingleWithMultiInput)
{
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h",
                          "/usr/include/net/ethernet.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/usr/include/net"};
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {
        {{"directories", {"actionbuilder", "deps", "usr"}}},
        {{"files", {"hello.cpp"}}},
        {{"files", {"empty.c"}}},
        {{"directories", {"include"}}},
        {{"files", {"ctype.h"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}

/**
 * Make sure that the working directory of the action exists in the
 * input root, regardless if there are any files in it.
 */
TEST_F(ActionBuilderTestFixture, EmptyWorkingDirInputRoot)
{
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"../deps/empty.c"};
    std::vector<std::string> recc_args = {"gcc", "-c", "../deps/empty.c", "-o",
                                          "empty.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                                {{"files", {}}},
                                {{"files", {"empty.c"}}}};
    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
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
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    ASSERT_EQ(actionPtr, nullptr);
}

/**
 * Force path replacement, and make sure paths are correctly replaced in the
 * resulting action.
 */
TEST_F(ActionBuilderTestFixture, SimplePathReplacement)
{
    // Replace all paths to /usr/include to /usr
    RECC_PREFIX_REPLACEMENT = {{"/usr/include", "/usr"}};

    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();
    MerkleTree expected_tree = {
        {{"files", {"hello.cpp"}}, {"directories", {"usr"}}},
        {{"files", {"ctype.h"}}}};

    verify_merkle_tree(current_digest, expected_tree.begin(),
                       expected_tree.end(), blobs);
}