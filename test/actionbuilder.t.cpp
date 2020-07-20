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
#include <buildboxcommon_fileutils.h>
#include <buildboxcommonmetrics_durationmetricvalue.h>
#include <buildboxcommonmetrics_testingutils.h>
#include <digestgenerator.h>
#include <env.h>
#include <fileutils.h>
#include <fstream>
#include <protos.h>

#include <gtest/gtest.h>

#define TIMER_NAME_COMPILER_DEPS "recc.compiler_deps"
#define TIMER_NAME_BUILD_MERKLE_TREE "recc.build_merkle_tree"

using namespace BloombergLP::recc;
using namespace buildboxcommon::buildboxcommonmetrics;

class ActionBuilderTestFixture
    : public ActionBuilder,
      public ::testing::TestWithParam<const char *> {
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
        d_previous_reapi_version = RECC_REAPI_VERSION;
    }

    void TearDown() override
    {
        RECC_PREFIX_REPLACEMENT = d_previous_recc_replacement_map;
        RECC_DEPS_OVERRIDE = d_previous_deps_override;
        RECC_FORCE_REMOTE = d_previous_force_remote;
        RECC_DEPS_GLOBAL_PATHS = d_previous_deps_global_path;
        RECC_DEPS_EXCLUDE_PATHS = d_previous_deps_exclude_paths;
        RECC_WORKING_DIR_PREFIX = d_previous_working_dir_prefix;
        RECC_REAPI_VERSION = d_previous_reapi_version;
    }

    void writeDependenciesToTempFile(const std::string &dependency_file_name)
    {
        const std::string path_to_hello = cwd + "/" + "hello.cpp";
        // references the hello.cpp file in test/data/actionbuilder/hello.cpp
        const std::string make_rule = "hello.o: " + path_to_hello + "\n";
        std::ofstream fakedependencyfile;
        fakedependencyfile.open(dependency_file_name);
        fakedependencyfile << make_rule;
        fakedependencyfile.close();
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
    std::string d_previous_reapi_version;
};

TEST_F(ActionBuilderTestFixture, BuildSimpleCommand)
{
    const std::vector<std::string> command_arguments = {
        "/my/fake/gcc", "-c", "hello.cpp", "-o", "hello.o"};
    const std::set<std::string> output_files = {"hello.o"};
    const std::string working_directory = ".";

    const proto::Command command_proto = this->generateCommandProto(
        command_arguments, output_files, {}, {}, {}, working_directory);

    ASSERT_TRUE(std::equal(command_arguments.cbegin(),
                           command_arguments.cend(),
                           command_proto.arguments().cbegin()));

    ASSERT_TRUE(std::equal(output_files.cbegin(), output_files.cend(),
                           command_proto.output_files().cbegin()));

    ASSERT_EQ(command_proto.working_directory(), working_directory);

    ASSERT_TRUE(command_proto.environment_variables().empty());
    ASSERT_TRUE(command_proto.platform().properties().empty());
    ASSERT_TRUE(command_proto.output_directories().empty());
}

TEST_P(ActionBuilderTestFixture, BuildSimpleCommandWithOutputPaths)
{
    RECC_REAPI_VERSION = "2.1";

    const std::vector<std::string> command_arguments = {
        "/my/fake/gcc", "-c", "hello.cpp", "-o", "hello.o"};
    const std::set<std::string> output_files = {"hello.o"};
    const std::string working_directory = ".";

    const proto::Command command_proto = this->generateCommandProto(
        command_arguments, output_files, {}, {}, {}, working_directory);

    ASSERT_TRUE(std::equal(command_arguments.cbegin(),
                           command_arguments.cend(),
                           command_proto.arguments().cbegin()));

    ASSERT_EQ(command_proto.working_directory(), working_directory);
    ASSERT_TRUE(command_proto.environment_variables().empty());
    ASSERT_TRUE(command_proto.platform().properties().empty());

    ASSERT_EQ(command_proto.output_paths_size(), 1);
    ASSERT_EQ(command_proto.output_paths(0), "hello.o");

    ASSERT_TRUE(command_proto.output_files().empty());
    ASSERT_TRUE(command_proto.output_directories().empty());
}

TEST_F(ActionBuilderTestFixture, GetDependenciesAIX)
{
    const std::vector<std::string> recc_args = {
        "./xlc", "-c", cwd + "/" + "hello.cpp ", "-o", "hello.o"};
    std::set<std::string> deps;
    std::set<std::string> prod;
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    ASSERT_FALSE(command.get_aix_dependency_file_name().empty());

    writeDependenciesToTempFile(command.get_aix_dependency_file_name());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);
}

TEST_F(ActionBuilderTestFixture, IllegalNonCompileCommand)
{
    const std::vector<std::string> recc_args = {"command-with-no-path", "foo",
                                                "bar"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());

    ASSERT_TRUE(command.get_aix_dependency_file_name().empty());

    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_EQ(actionPtr, nullptr);
}

TEST_F(ActionBuilderTestFixture, IllegalCompileCommandThrows)
{
    const std::vector<std::string> recc_args = {"gcc", "-c", "hello.cpp", "-o",
                                                "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());

    EXPECT_THROW(ActionBuilder::BuildAction(command, cwd, &blobs,
                                            &digest_to_filecontents),
                 std::invalid_argument);
}

TEST_F(ActionBuilderTestFixture, BuildMerkleTreeVerifyMetricsCollection)
{
    DependencyPairs dep_pairs;
    buildMerkleTree(dep_pairs, "cwd", nullptr, nullptr);
    EXPECT_TRUE(
        collectedByName<DurationMetricValue>(TIMER_NAME_BUILD_MERKLE_TREE));
}

TEST_F(ActionBuilderTestFixture, GetDependenciesVerifyMetricsCollection)
{
    const std::vector<std::string> recc_args = {"./gcc", "-c", "hello.cpp",
                                                "-o", "hello.o"};
    std::set<std::string> deps;
    std::set<std::string> prod;
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    getDependencies(command, &deps, &prod);
    EXPECT_TRUE(
        collectedByName<DurationMetricValue>(TIMER_NAME_COMPILER_DEPS));
}

TEST_F(ActionBuilderTestFixture, OutputFilesAndDirectories)
{
    RECC_REAPI_VERSION = "2.0";

    const std::set<std::string> output_files = {"hello.o", "foo.o", "bar.o"};
    const std::set<std::string> output_directories = {"export/"};

    const proto::Command command_proto = this->generateCommandProto(
        {}, output_files, output_directories, {}, {}, "");

    ASSERT_TRUE(std::equal(output_files.cbegin(), output_files.cend(),
                           command_proto.output_files().cbegin()));

    ASSERT_TRUE(std::equal(output_directories.cbegin(),
                           output_directories.cend(),
                           command_proto.output_directories().cbegin()));

    ASSERT_TRUE(command_proto.output_paths().empty());
}

TEST_P(ActionBuilderTestFixture, OutputPaths)
{
    RECC_REAPI_VERSION = "2.1";

    const std::set<std::string> output_files = {"hello.o", "foo.o", "bar.o"};
    const std::set<std::string> output_directories = {"export/"};

    const proto::Command command_proto = this->generateCommandProto(
        {}, output_files, output_directories, {}, {}, "");

    const std::vector<std::string> expectedOutputPaths = {"hello.o", "foo.o",
                                                          "bar.o", "export/"};

    ASSERT_TRUE(std::is_permutation(expectedOutputPaths.cbegin(),
                                    expectedOutputPaths.cend(),
                                    command_proto.output_paths().cbegin()));

    ASSERT_TRUE(command_proto.output_files().empty());
    ASSERT_TRUE(command_proto.output_directories().empty());
}

TEST_F(ActionBuilderTestFixture, EnvironmentVariables)
{
    std::map<std::string, std::string> environment;
    environment["PATH"] = "/usr/bin:/opt/bin:/bin";
    environment["GREETING"] = "hello!";

    const proto::Command command_proto =
        this->generateCommandProto({}, {}, {}, environment, {}, "");

    ASSERT_EQ(command_proto.environment_variables_size(), 2);
    for (const auto &variable : command_proto.environment_variables()) {
        ASSERT_TRUE(environment.count(variable.name()));
        ASSERT_EQ(variable.value(), environment.at(variable.name()));
    }
}

TEST_F(ActionBuilderTestFixture, PlatformProperties)
{
    std::map<std::string, std::string> platform_properties;
    platform_properties["OS"] = "linux";
    platform_properties["ISA"] = "x86";

    const proto::Command command_proto =
        this->generateCommandProto({}, {}, {}, {}, platform_properties, "");

    ASSERT_EQ(command_proto.platform().properties_size(), 2);
    for (const auto &property : command_proto.platform().properties()) {
        ASSERT_TRUE(platform_properties.count(property.name()));
        ASSERT_EQ(property.value(), platform_properties.at(property.name()));
    }
}

TEST_F(ActionBuilderTestFixture, WorkingDirectory)
{
    // Working directories are not manipulated:
    const auto paths = {"..", "/home/user/dev", "./subdir1/subdir2"};
    for (const auto &working_directory : paths) {
        const proto::Command command_proto =
            this->generateCommandProto({}, {}, {}, {}, {}, working_directory);
        ASSERT_EQ(working_directory, command_proto.working_directory());
    }
}

TEST_F(ActionBuilderTestFixture, PathsArePrefixed)
{
    RECC_PREFIX_REPLACEMENT = {{"/usr/bin", "/opt"}};
    const auto working_directory = "/usr/bin";

    const proto::Command command_proto =
        this->generateCommandProto({}, {}, {}, {}, {}, working_directory);

    ASSERT_EQ(command_proto.working_directory(), "/opt");
}

TEST_F(ActionBuilderTestFixture, CommonAncestorPath1)
{
    const DependencyPairs dependencies = {std::make_pair("dep.c", "dep.c")};
    const std::set<std::string> output_paths = {"../build/"};
    const std::string working_directory = "/tmp/workspace/";

    const auto commonAncestor =
        commonAncestorPath(dependencies, output_paths, working_directory);
    ASSERT_EQ(commonAncestor, "workspace");
}

TEST_F(ActionBuilderTestFixture, CommonAncestorPath2)
{
    const DependencyPairs dependencies = {std::make_pair("dep.c", "dep.c")};
    const std::set<std::string> output_paths = {"../../build/"};
    const std::string working_directory = "/tmp/workspace/";

    const auto commonAncestor =
        commonAncestorPath(dependencies, output_paths, working_directory);
    ASSERT_EQ(commonAncestor, "tmp/workspace");
}

TEST_F(ActionBuilderTestFixture, CommonAncestorPath3)
{
    const DependencyPairs dependencies = {
        std::make_pair("src/dep.c", "src/dep.c")};
    const std::set<std::string> output_paths = {"build/out/"};
    const std::string working_directory = "/tmp/workspace/";

    const auto commonAncestor =
        commonAncestorPath(dependencies, output_paths, working_directory);
    ASSERT_EQ(commonAncestor, "");
}

TEST_F(ActionBuilderTestFixture, WorkingDirectoryPrefixEmptyPrefix)
{
    for (const auto &path : {"dir/", "/tmp/subdir"}) {
        ASSERT_EQ(path, prefixWorkingDirectory(path, ""));
    }
}

TEST_F(ActionBuilderTestFixture, WorkingDirectoryPrefix)
{
    const auto prefix = "/home/user/dev";

    ASSERT_EQ(prefixWorkingDirectory("dir/", prefix), "/home/user/dev/dir/");
    ASSERT_EQ(prefixWorkingDirectory("tmp/subdir", prefix),
              "/home/user/dev/tmp/subdir");
}

TEST_P(ActionBuilderTestFixture, ActionContainsExpectedCompileCommand)
{
    const std::string working_dir_prefix = GetParam();
    RECC_WORKING_DIR_PREFIX = working_dir_prefix;

    const std::vector<std::string> recc_args = {"./gcc", "-c", "hello.cpp",
                                                "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());

    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    ASSERT_NE(actionPtr, nullptr);

    proto::Command expected_command;
    for (const auto &arg : recc_args) {
        expected_command.add_arguments(arg);
    }
    expected_command.add_output_files("hello.o");
    expected_command.set_working_directory(working_dir_prefix);

    ASSERT_EQ(actionPtr->command_digest(),
              DigestGenerator::make_digest(expected_command));
}

TEST_P(ActionBuilderTestFixture, ActionCompileCommandGoldenDigests)
{
    const std::string working_dir_prefix = GetParam();
    RECC_WORKING_DIR_PREFIX = working_dir_prefix;

    const std::vector<std::string> recc_args = {"./gcc", "-c", "hello.cpp",
                                                "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());

    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    /** Modifying these hash values should be done carefully and only if
     * absolutely required.
     */
    if (working_dir_prefix.empty()) {
        EXPECT_EQ(
            "2751b2f369c793ec389e54410db4b2eeddbcbbff3adc004e4283b81f254ffa14",
            actionPtr->command_digest().hash());
    }
    else {
        EXPECT_EQ(
            "df731385d14c9c5080f1cc8e84a41c0ec9391b88d3b0e11a05e63835be03e30b",
            actionPtr->command_digest().hash());
    }
}

TEST_P(ActionBuilderTestFixture, ActionNonCompileCommandGoldenDigests)
{
    /* [!] WARNING [!]
     * Changes that make this test fail will produce caches misses.
     */

    const std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }

    RECC_FORCE_REMOTE = true;

    const std::vector<std::string> recc_args = {"/bin/ls"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    /** Modifying these hash values should be done carefully and only if
     * absolutely required.
     */
    if (working_dir_prefix.empty()) {
        EXPECT_EQ(
            "87c8a7778c3fc31f5b15bbe34d5f49e0d6bb1dbcc00551e8f14927bf23fe6536",
            actionPtr->command_digest().hash());
    }
    else {
        EXPECT_EQ(
            "1a54c359f6bcdcc7f70fd201645bd829c8c689c77af5de5d0104d31490563dad",
            actionPtr->command_digest().hash());
    }
}

// This is a representation of a flat merkle tree, where things are stored in
// the required order of traversal.
typedef std::vector<std::unordered_map<std::string, std::vector<std::string>>>
    MerkleTree;

// Recursively verify that a merkle tree matches an expected input layout.
// This doesn't look at the hashes, just that the declared layout matches
void verify_merkle_tree(const proto::Digest &digest,
                        const MerkleTree &expected, size_t &index, size_t end,
                        const digest_string_umap &blobs)
{
    ASSERT_LE(index, end) << "Reached end of expected output early";
    auto currentLevel = expected[index];
    const auto current_blob = blobs.find(digest);
    ASSERT_NE(current_blob, blobs.end())
        << "No blob found for digest " << digest.hash();

    proto::Directory directory;
    ASSERT_TRUE(directory.ParseFromString(current_blob->second));

    // Exit early if there are more/less files or dirs in the given tree than
    // expected
    ASSERT_EQ(directory.files().size(), currentLevel["files"].size())
        << "Wrong number of files at current level";
    ASSERT_EQ(directory.directories().size(),
              currentLevel["directories"].size())
        << "Wrong number of directories at current level";

    size_t f_index = 0;
    for (const auto &file : directory.files()) {
        ASSERT_EQ(file.name(), currentLevel["files"][f_index])
            << "Wrong file found";
        f_index++;
    }

    size_t d_index = 0;
    for (const auto &subdirectory : directory.directories()) {
        ASSERT_EQ(subdirectory.name(), currentLevel["directories"][d_index])
            << "Wrong directory found";
        d_index++;
    }

    // All the files/directories at this level are correct, now check all the
    // subdirectories
    for (const auto &subdirectory : directory.directories()) {
        verify_merkle_tree(subdirectory.digest(), expected, ++index, end,
                           blobs);
    }
}

/**
 * Verify that the generated Action has the correct working directory
 */
void verify_working_directory(const proto::Digest &digest,
                              const std::string &expected_workdir,
                              const digest_string_umap &blobs)
{
    proto::Command command_proto;
    const auto command_blob = blobs.find(digest);
    ASSERT_TRUE(command_proto.ParseFromString(command_blob->second));
    EXPECT_EQ(command_proto.working_directory(), expected_workdir);
}

/**
 * Builds an action for test/data/actionbuilder/hello.cpp and checks against
 * precomputed digest values, as well as checks the structure of the input_root
 */
TEST_P(ActionBuilderTestFixture, ActionContainsExpectedMerkleTree)
{
    const std::string working_dir_prefix = GetParam();

    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }
    RECC_DEPS_OVERRIDE = {"hello.cpp"};
    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());

    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    ASSERT_NE(actionPtr, nullptr);

    MerkleTree expected_tree;
    if (working_dir_prefix.empty()) {
        EXPECT_EQ(
            "05a73e8a93752c197cb58e4bdb43bd1d696fa02ecff899a339c4ecacbab2c14b",
            actionPtr->input_root_digest().hash());
        // The expected result is a single file at the root
        expected_tree = {{{"files", {"hello.cpp"}}}};
    }
    else {
        EXPECT_EQ(
            "edd3e49326719324c5af290089e30ec58f11dd9eb2feb029930c95fb8ca32eea",
            actionPtr->input_root_digest().hash());

        // The expected result is a single file, under the working_dir_prefix
        // directory
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"files", {"hello.cpp"}}}};
    }
    // Check the layout of the input_root is what we expect
    size_t startIndex = 0;
    verify_merkle_tree(actionPtr->input_root_digest(), expected_tree,
                       startIndex, expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ActionBuiltGoldenDigests)
{
    const std::string working_dir_prefix = GetParam();

    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }

    const std::vector<std::string> recc_args = {"./gcc", "-c", "hello.cpp",
                                                "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());

    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    ASSERT_NE(actionPtr, nullptr);

    const auto actionDigest = DigestGenerator::make_digest(*actionPtr);
    if (working_dir_prefix.empty()) {
        EXPECT_EQ(140, actionDigest.size_bytes());
        EXPECT_EQ(
            "6a0949be163692a6a9e51530e31a5c3721246f350fe2d6c00e5cfd90d749e6b3",
            actionDigest.hash());
    }
    else {
        EXPECT_EQ(140, actionDigest.size_bytes());
        EXPECT_EQ(
            "1ce0771dfafdd22d59bdf0ac6a6c9fd6e00a183b0cd17fc09fec0bad1b460345",
            actionDigest.hash());
    }
}

/**
 * Non-compile commands return nullptrs, indicating that they are intended to
 * be run locally if RECC_FORCE_REMOTE isn't set
 */
TEST_P(ActionBuilderTestFixture, NonCompileCommand)
{
    const std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }

    RECC_FORCE_REMOTE = false;

    const std::vector<std::string> recc_args = {"ls"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    ASSERT_EQ(actionPtr, nullptr);
}

/**
 * However, force remoting a non-compile command still generates an action
 */
TEST_P(ActionBuilderTestFixture, NonCompileCommandForceRemoteMerkleTree)
{
    const std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }

    RECC_FORCE_REMOTE = true;

    const std::vector<std::string> recc_args = {"/bin/ls"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        EXPECT_EQ(
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            actionPtr->input_root_digest().hash());
        // No input files, so the tree/working-dir is empty
        expected_tree = {{{}}};
        expected_working_dir = "";
    }
    else {
        EXPECT_EQ(
            "eadd312151cd61b24291b3f0d4ec461476354e8147379246c7024903cf17cdbb",
            actionPtr->input_root_digest().hash());

        // Expect a single top level directory equal to RECC_WORKING_DIR_PREFIX
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"files", {}}}};
        expected_working_dir = working_dir_prefix;
    }

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);
    // Check the layout of the input_root is what we expect
    size_t startIndex = 0;
    verify_merkle_tree(actionPtr->input_root_digest(), expected_tree,
                       startIndex, expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, NonCompileCommandForceRemoteGoldenDigests)
{
    const std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }

    RECC_FORCE_REMOTE = true;

    const std::vector<std::string> recc_args = {"/bin/ls"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    const auto digest = DigestGenerator::make_digest(*actionPtr);
    if (working_dir_prefix.empty()) {
        EXPECT_EQ(138, digest.size_bytes());
        EXPECT_EQ(
            "fed2b8c46fe8c65c4893272eedade57ab20f7aab728eeaaade9931df9062313e",
            digest.hash());
    }
    else {
        EXPECT_EQ(140, digest.size_bytes());
        EXPECT_EQ(
            "823044cf5ff94e3b4f86f8224ca331d3cc19f91f7e36a197f041895233514e0d",
            digest.hash());
    }
}

/**
 * Make sure an absolute file dependencies are represented properly. Do this by
 * using RECC_DEPS_OVERRIDE to return a deterministic set of dependencies.
 * Otherwise things are platform dependant
 */
TEST_P(ActionBuilderTestFixture, AbsolutePathActionBuilt)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"files", {"hello.cpp"}}, {"directories", {"usr"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = "";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = working_dir_prefix;
    }
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    // Since this test unfortunately relies on the hash of a system installed
    // file, it can't compare the input root digest. Instead go through the
    // merkle tree and verify each level has the correct layout.

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
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
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps", "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = "actionbuilder";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = working_dir_prefix + "/actionbuilder";
    }
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePath)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
        expected_working_dir = "actionbuilder";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
        expected_working_dir = working_dir_prefix + "/actionbuilder";
    }
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/usr/include"};

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePathMultipleMatchOne)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
        expected_working_dir = "actionbuilder";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}}};
        expected_working_dir = working_dir_prefix + "/actionbuilder";
    }

    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/foo/bar", "/usr/include"};

    std::vector<std::string> recc_args = {"/my/fake/gcc", "-c", "hello.cpp",
                                          "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePathNoMatch)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps", "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = "actionbuilder";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = working_dir_prefix + "/actionbuilder";
    }
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/foo/bar"};

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

TEST_P(ActionBuilderTestFixture, ExcludePathSingleWithMultiInput)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps", "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = "actionbuilder";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"empty.c"}}},
                         {{"directories", {"include"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = working_dir_prefix + "/actionbuilder";
    }

    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h",
                          "/usr/include/net/ethernet.h", "../deps/empty.c",
                          "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;
    RECC_DEPS_EXCLUDE_PATHS = {"/usr/include/net"};

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
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
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {}}},
                         {{"files", {"empty.c"}}}};
        expected_working_dir = "actionbuilder";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"directories", {"actionbuilder", "deps"}}},
                         {{"files", {}}},
                         {{"files", {"empty.c"}}}};
        expected_working_dir = working_dir_prefix + "/actionbuilder";
    }
    cwd = FileUtils::getCurrentWorkingDirectory();
    RECC_DEPS_OVERRIDE = {"../deps/empty.c"};

    const std::vector<std::string> recc_args = {
        "/my/fake/gcc", "-c", "../deps/empty.c", "-o", "empty.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();

    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

/**
 * Trying to output to an unrelated directory returns a nullptr, forcing a
 * local execution.
 */
TEST_P(ActionBuilderTestFixture, UnrelatedOutputDirectory)
{
    const std::string working_dir_prefix = GetParam();
    if (!working_dir_prefix.empty()) {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
    }
    const std::vector<std::string> recc_args = {"./gcc", "-c", "hello.cpp",
                                                "-o", "/fakedirname/hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);

    ASSERT_EQ(actionPtr, nullptr);
}

/**
 * Force path replacement, and make sure paths are correctly replaced in the
 * resulting action.
 */
TEST_P(ActionBuilderTestFixture, SimplePathReplacement)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"files", {"hello.cpp"}}, {"directories", {"usr"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = "";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix, "usr"}}},
                         {{"files", {"hello.cpp"}}},
                         {{"files", {"ctype.h"}}}};
        expected_working_dir = working_dir_prefix;
    }

    // Replace all paths to /usr/include to /usr
    RECC_PREFIX_REPLACEMENT = {{"/usr/include", "/usr"}};

    RECC_DEPS_OVERRIDE = {"/usr/include/ctype.h", "hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

/**
 * Test that absolute paths are made relative to RECC_PROJECT_ROOT
 * and that if a working_dir_prefix is specified that it is prepended
 */
TEST_P(ActionBuilderTestFixture, PathsMadeRelativeToProjectRoot)
{
    const std::string working_dir_prefix = GetParam();

    MerkleTree expected_tree;
    std::string expected_working_dir;
    if (working_dir_prefix.empty()) {
        expected_tree = {{{"files", {"hello.cpp"}}}};
        expected_working_dir = "";
    }
    else {
        RECC_WORKING_DIR_PREFIX = working_dir_prefix;
        expected_tree = {{{"directories", {working_dir_prefix}}},
                         {{"files", {"hello.cpp"}}}};
        expected_working_dir = working_dir_prefix;
    }

    RECC_PROJECT_ROOT = cwd;
    RECC_DEPS_OVERRIDE = {cwd + "/hello.cpp"};
    RECC_DEPS_GLOBAL_PATHS = 1;

    const std::vector<std::string> recc_args = {"/my/fake/gcc", "-c",
                                                "hello.cpp", "-o", "hello.o"};
    const auto command =
        ParsedCommandFactory::createParsedCommand(recc_args, cwd.c_str());
    const auto actionPtr = ActionBuilder::BuildAction(command, cwd, &blobs,
                                                      &digest_to_filecontents);
    ASSERT_NE(actionPtr, nullptr);

    // Verify the command working directory matches
    verify_working_directory(actionPtr->command_digest(), expected_working_dir,
                             blobs);

    const auto current_digest = actionPtr->input_root_digest();
    size_t startIndex = 0;
    verify_merkle_tree(current_digest, expected_tree, startIndex,
                       expected_tree.size(), blobs);
}

// Run each test twice, once with no working_dir_prefix set, and one with
// it set to "recc-build"
INSTANTIATE_TEST_CASE_P(ActionBuilder, ActionBuilderTestFixture,
                        testing::Values("", "recc-build"));
