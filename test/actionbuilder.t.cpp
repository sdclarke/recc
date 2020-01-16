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

class ActionBuilderTestFixture
    : public ::testing::TestWithParam<const char *> {
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
        d_previous_working_dir_prefix = RECC_WORKING_DIR_PREFIX;
    }

    void TearDown() override
    {
        RECC_PREFIX_REPLACEMENT = d_previous_recc_replacement_map;
        RECC_DEPS_OVERRIDE = d_previous_deps_override;
        RECC_FORCE_REMOTE = d_previous_force_remote;
        RECC_DEPS_GLOBAL_PATHS = d_previous_deps_global_path;
        RECC_DEPS_EXCLUDE_PATHS = d_previous_deps_exclude_paths;
        RECC_WORKING_DIR_PREFIX = d_previous_working_dir_prefix;
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
    std::string d_previous_working_dir_prefix;
};

// This is a representation of a flat merkle tree, where things are stored in
// the required order of traversal.
typedef std::vector<std::unordered_map<std::string, std::vector<std::string>>>
    MerkleTree;

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
void verify_merkle_tree(proto::Digest digest, MerkleTree &expected, int &index,
                        int end, digest_string_umap blobs)
{
    ASSERT_LE(index, end) << "Reached end of expected output early";
    auto currentLevel = expected[index];
    proto::Directory directory;
    auto current_blob = blobs.find(digest);
    ASSERT_NE(current_blob, blobs.end())
        << "No blob found for digest " << digest.hash();

    directory.ParseFromString(current_blob->second);

    // Exit early if there are more/less files or dirs in the given tree than
    // expected
    ASSERT_EQ(directory.files().size(), currentLevel["files"].size())
        << "Wrong number of files at current level";
    ASSERT_EQ(directory.directories().size(),
              currentLevel["directories"].size())
        << "Wrong number of directories at current level";

    int f_index = 0;
    for (auto &file : directory.files()) {
        ASSERT_EQ(file.name(), currentLevel["files"][f_index])
            << "Wrong file found";
        f_index++;
    }
    int d_index = 0;
    for (auto &subdirectory : directory.directories()) {
        ASSERT_EQ(subdirectory.name(), currentLevel["directories"][d_index])
            << "Wrong directory found";
        d_index++;
    }
    // All the files/directories at this level are correct, now check all the
    // subdirectories
    for (auto &subdirectory : directory.directories()) {
        verify_merkle_tree(subdirectory.digest(), expected, ++index, end,
                           blobs);
    }
}

/**
 * Builds an action for test/data/actionbuilder/hello.cpp and checks against
 * precomputed digest values, as well as checks the structure of the input_root
 */
TEST_P(ActionBuilderTestFixture, ActionBuilt)
{
    std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }
    std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                          "hello.o"};
    ParsedCommand command(recc_args, cwd.c_str());

    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    if (!actionPtr) {
        fail_to_build_action(recc_args);
    }
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
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

        // The expected result is a single file at the root
        expected_tree = {{{"files", {"hello.cpp"}}}};
    }
    else {
        EXPECT_EQ(
            "c80a0f815fd896c301aeab7e4a24c86d73a2893fbc6bffdd651459c157ed5b3f",
            actionPtr->command_digest().hash());

        EXPECT_EQ(
            "edd3e49326719324c5af290089e30ec58f11dd9eb2feb029930c95fb8ca32eea",
            actionPtr->input_root_digest().hash());

        auto digest = DigestGenerator::make_digest(*actionPtr);
        EXPECT_EQ(140, digest.size_bytes());
        EXPECT_EQ(
            "f77bd8c2492fee3f9c8655381c6912e2dbbd264672a88fe81b18aa77fd1f7707",
            digest.hash());

        // The expected result is a single file, under the working_dir_prefix
        // directory
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"files", {"hello.cpp"}}}};
    }
    // Check the layout of the input_root is what we expect
    int startIndex = 0;
    verify_merkle_tree(actionPtr->input_root_digest(), expected_tree,
                       startIndex, expected_tree.size(), blobs);
}

/**
 * Non-compile commands return nullptrs, indicating that they are intended to
 * be run locally if RECC_FORCE_REMOTE isn't set
 */
TEST_P(ActionBuilderTestFixture, NonCompileCommand)
{
    std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }
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
TEST_P(ActionBuilderTestFixture, NonCompileCommandForceRemote)
{
    std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }
    RECC_FORCE_REMOTE = true;
    std::vector<std::string> recc_args = {"ls"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    if (!actionPtr) {
        fail_to_build_action(recc_args);
    }
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
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

        // No input files, so the tree is empty
        expected_tree = {{{}}};
    }
    else {
        EXPECT_EQ(
            "5fad9bc2846b1fa419e66b9614643cc3e384889b9e9626b98a40bc125861bc45",
            actionPtr->command_digest().hash());

        EXPECT_EQ(
            "eadd312151cd61b24291b3f0d4ec461476354e8147379246c7024903cf17cdbb",
            actionPtr->input_root_digest().hash());

        auto digest = DigestGenerator::make_digest(*actionPtr);

        EXPECT_EQ(140, digest.size_bytes());
        EXPECT_EQ(
            "14b83188ea494940aa35d746fd385a08ed63e174be55cce2ada897ee8c4eaf9a",
            digest.hash());

        // Expect a single top level directory equal to RECC_WORKING_DIR_PREFIX
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"files", {}}}};
    }
    // Check the layout of the input_root is what we expect
    int startIndex = 0;
    verify_merkle_tree(actionPtr->input_root_digest(), expected_tree,
                       startIndex, expected_tree.size(), blobs);
}

/**
 * Make sure an absolute file dependencies are represented properly. Do this by
 * using RECC_DEPS_OVERRIDE to return a deterministic set of dependencies.
 * Otherwise things are platform dependant
 */
TEST_P(ActionBuilderTestFixture, AbsolutePathActionBuilt)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"files", {"hello.cpp"}}, {"directories", {"usr"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

/**
 * Test to make sure that paths with .. are properly resolved using
 * information from the current directory, and that it doesn't cause
 * issues with absolute paths
 */
TEST_P(ActionBuilderTestFixture, RelativePathAndAbsolutePathWithCwd)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps", "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePath)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePathMultipleMatchOne)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePathNoMatch)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps", "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePathSingleWithMultiInput)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps", "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

/**
 * Make sure that the working directory of the action exists in the
 * input root, regardless if there are any files in it.
 */
TEST_P(ActionBuilderTestFixture, EmptyWorkingDirInputRoot)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {}}},
                         {{"files", {"empty.c"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {}}},
                         {{"files", {"empty.c"}}}};
    }
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"../deps/empty.c"};
    std::vector<std::string> recc_args = {"gcc", "-c", "../deps/empty.c", "-o",
                                          "empty.o"};
    ParsedCommand command(recc_args, cwd.c_str());
    auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                &digest_to_filecontents);

    auto current_digest = actionPtr->input_root_digest();

    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

/**
 * Trying to output to an unrelated directory returns a nullptr, forcing a
 * local execution.
 */
TEST_P(ActionBuilderTestFixture, UnrelatedOutputDirectory)
{
    std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }
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
TEST_P(ActionBuilderTestFixture, SimplePathReplacement)
{
    std::string working_dir_prefix = GetParam();
    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"files", {"hello.cpp"}}, {"directories", {"usr"}}},
                         {{"files", {"ctype.h"}}}};
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"ctype.h"}}}};
    }
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
    int startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

// Run each test twice, once with no working_dir_prefix set, and one with
// it set to "recc-build"
INSTANTIATE_TEST_CASE_P(ActionBuilder, ActionBuilderTestFixture,
                        testing::Values("", "recc-build"));
