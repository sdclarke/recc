// Copyright 2020 Bloomberg Finance L.P
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

#include <compilerdefaults.h>
#include <env.h>
#include <gtest/gtest.h>
#include <parsedcommand.h>
#include <parsedcommandfactory.h>

using namespace BloombergLP::recc;

TEST(VectorFromArgvTest, EmptyArgv)
{
    const char *argv[] = {nullptr};
    EXPECT_EQ(ParsedCommandFactory::vectorFromArgv(argv),
              std::vector<std::string>());
}

TEST(VectorFromArgvTest, OneItemArgv)
{
    const char *argv[] = {"gcc", nullptr};
    std::vector<std::string> expected = {"gcc"};

    EXPECT_EQ(ParsedCommandFactory::vectorFromArgv(argv), expected);
}

TEST(VectorFromArgvTest, MultiItemArgv)
{
    const char *argv[] = {"test", "", "of long", "argv", nullptr};
    std::vector<std::string> expected = {"test", "", "of long", "argv"};

    EXPECT_EQ(ParsedCommandFactory::vectorFromArgv(argv), expected);
}

TEST(TestParsedCommandFactory, emptyCommand)
{
    const std::vector<std::string> command = {};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {};

    // Change once deps are handled.
    const std::vector<std::string> expectedDepsCommand = {};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(false, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testInputPathsAndGCC)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "test/hello.c", "-Itest/include/user.h"};

    // Change once deps are handled.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h", "-M"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(true, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testEqualInputPaths)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h", "--sysroot=/home/nobody/test"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "test/hello.c", "-Itest/include/user.h",
        "--sysroot=test"};

    // Change once deps are handled.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h",
        "--sysroot=/home/nobody/test",
        "-M"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
}

TEST(TestParsedCommandFactory, testPreprocessor)
{
    RECC_PROJECT_ROOT = "/home/nobody/";

    const std::vector<std::string> command = {
        "gcc",
        "-c",
        "/home/nobody/test/hello.c",
        "-o",
        "/home/nobody/test/hello.o",
        "-I/home/nobody/headers",
        "-I",
        "/home/nobody/test/moreheaders/",
        "-Wp,-I,/home/nobody/evenmoreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/usr/include/something"};

    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        command, "/home/nobody/test");

    const std::vector<std::string> expectedCommand = {
        "gcc",
        "-c",
        "hello.c",
        "-o",
        "hello.o",
        "-I../headers",
        "-I",
        "moreheaders/",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "../evenmoreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/usr/include/something"};

    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/headers",
        "-I",
        "/home/nobody/test/moreheaders/",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/home/nobody/evenmoreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/usr/include/something",
        "-M"};
    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(true, parsedCommand.is_compiler_command());
}

TEST(PathReplacement, modifyRemotePathUnmodified)
{
    // If a given path doesn't match any PREFIX_REPLACEMENT
    // rules and can't be made relative, it's returned unmodified
    RECC_PROJECT_ROOT = "/home/nobody/";
    RECC_PREFIX_REPLACEMENT = {{"/home", "/hi"}};

    const auto workingDir = "/home";

    const std::string replacedPath = ParsedCommandModifiers::modifyRemotePath(
        "/other/dir/nobody/test", workingDir);

    EXPECT_EQ("/other/dir/nobody/test", replacedPath);
}

TEST(PathReplacement, modifyRemotePathPrefixMatch)
{
    // Match a PREFIX_REPLACEMENT rule, but the replaced path
    // isn't eligable to be made relative, so it's returned absolute
    RECC_PROJECT_ROOT = "/home/nobody/";
    RECC_PREFIX_REPLACEMENT = {{"/home", "/hi"}};

    const auto workingDir = "/home";

    const std::string replacedPath = ParsedCommandModifiers::modifyRemotePath(
        "/home/nobody/test", workingDir);

    EXPECT_EQ("/hi/nobody/test", replacedPath);
}

TEST(PathReplacement, modifyRemotePathMadeRelative)
{
    // Path doesn't match any PREFIX_REPLACEMENT rules,
    // but can be made relative to RECC_PROJECT_ROOT
    RECC_PROJECT_ROOT = "/other";
    RECC_PREFIX_REPLACEMENT = {{"/home", "/hi"}};

    const auto workingDir = "/other";

    const std::string replacedPath = ParsedCommandModifiers::modifyRemotePath(
        "/other/nobody/test", workingDir);

    EXPECT_EQ("nobody/test", replacedPath);
}

TEST(PathReplacement, modifyRemotePathPrefixAndRelativeMatch)
{
    // Path matches a PREFIX_REPLACEMENT rule, and the replaced
    // path can be made relative to RECC_PROJECT_ROOT
    RECC_PROJECT_ROOT = "/home/";
    RECC_PREFIX_REPLACEMENT = {{"/home/nobody/", "/home"}};

    const auto workingDir = "/home";

    const std::string replacedPath = ParsedCommandModifiers::modifyRemotePath(
        "/home/nobody/test", workingDir);

    EXPECT_EQ("test", replacedPath);
}
