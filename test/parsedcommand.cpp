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
#include <parsedcommand.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;

TEST(VectorFromArgvTest, EmptyArgv)
{
    const char *argv[] = {nullptr};
    EXPECT_EQ(vector_from_argv(argv), std::vector<std::string>());
}

TEST(VectorFromArgvTest, OneItemArgv)
{
    const char *argv[] = {"gcc", nullptr};
    std::vector<std::string> expected = {"gcc"};

    EXPECT_EQ(vector_from_argv(argv), expected);
}

TEST(VectorFromArgvTest, MultiItemArgv)
{
    const char *argv[] = {"test", "", "of long", "argv", nullptr};
    std::vector<std::string> expected = {"test", "", "of long", "argv"};

    EXPECT_EQ(vector_from_argv(argv), expected);
}

TEST(CommandBasenameTest, EmptyString) { EXPECT_EQ(command_basename(""), ""); }

TEST(CommandBasenameTest, TrivialCommands)
{
    EXPECT_EQ(command_basename("gcc"), "gcc");
    EXPECT_EQ(command_basename("CC"), "CC");
}

TEST(CommandBasenameTest, CommandsWithVersions)
{
    EXPECT_EQ(command_basename("gcc-4.7"), "gcc");
    EXPECT_EQ(command_basename("CC++-99"), "CC++");
}

TEST(CommandBasenameTest, CommandsAtPaths)
{
    EXPECT_EQ(command_basename("/usr/bin/gcc"), "gcc");
    EXPECT_EQ(command_basename("/CC++-99"), "CC++");
}

TEST(CommandBasenameTest, XlcVersions)
{
    EXPECT_EQ(command_basename("xlC128_r"), "xlC");
    EXPECT_EQ(command_basename("xlc++_r7"), "xlc++");
}

TEST(IsCompilerTest, Empty)
{
    ParsedCommand command = {};
    EXPECT_FALSE(command.is_compiler_command());
}

TEST(IsCompilerTest, CompilerParsedCommandPath)
{
    ParsedCommand command = {"/usr/local/bin/gcc", "-c", "test/file.cpp"};
    EXPECT_TRUE(command.is_compiler_command());
}

TEST(IsCompilerTest, NotCompiler)
{
    ParsedCommand command = {"cat", "something.c"};
    EXPECT_FALSE(command.is_compiler_command());
}

TEST(IsCompilerTest, EmptyString)
{
    ParsedCommand command = {""};
    EXPECT_FALSE(command.is_compiler_command());
}

TEST(GccTest, NonCompilerCommands)
{
    EXPECT_FALSE(ParsedCommand({"gcc"}).is_compiler_command());
    EXPECT_FALSE(ParsedCommand({"gcc", "-version"}).is_compiler_command());
    EXPECT_FALSE(ParsedCommand({"gcc", "hello.c", "-o", "hello"})
                     .is_compiler_command());
}

TEST(GccTest, SimpleCommand)
{
    ParsedCommand command = {"gcc", "-c", "hello.c"};
    std::vector<std::string> expectedCommand = {"gcc", "-c", "hello.c"};
    std::vector<std::string> expectedDepsCommand = {"gcc", "-c", "hello.c",
                                                    "-M"};
    std::set<std::string> expectedProducts = {};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccTest, OutputArgument)
{
    ParsedCommand command = {"gcc", "-c", "hello.c", "-o", "hello.o"};
    std::vector<std::string> expectedCommand = {"gcc", "-c", "hello.c", "-o",
                                                "hello.o"};
    std::vector<std::string> expectedDepsCommand = {"gcc", "-c", "hello.c",
                                                    "-M"};
    std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccTest, OutputArgumentNoSpace)
{
    ParsedCommand command = {"gcc", "-c", "-ohello.o", "hello.c"};
    std::vector<std::string> expectedCommand = {"gcc", "-c", "-ohello.o",
                                                "hello.c"};
    std::vector<std::string> expectedDepsCommand = {"gcc", "-c", "hello.c",
                                                    "-M"};
    std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccTest, PreprocessorArguments)
{
    ParsedCommand command = {"gcc",      "-c",      "-Xpreprocessor",
                             "-MMD",     "hello.c", "-Wp,hello.d,-MV,-valid",
                             "-ohello.o"};
    std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "hello.c", "-Xpreprocessor", "-valid", "-M"};
    std::set<std::string> expectedProducts = {"hello.o", "hello.d"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(SolarisCCTest, NonCompilerCommands)
{
    EXPECT_FALSE(ParsedCommand({"CC"}).is_compiler_command());
    EXPECT_FALSE(
        ParsedCommand({"CC", "hello.c", "-o", "hello"}).is_compiler_command());
    EXPECT_FALSE(
        ParsedCommand({"CC", "hello.c", "-c", "-###"}).is_compiler_command());
    EXPECT_FALSE(ParsedCommand({"CC", "hello.c", "-c", "-xprofile=test"})
                     .is_compiler_command());
}

TEST(SolarisCCTest, OutputArgument)
{
    ParsedCommand command = {"CC", "-c", "hello.c", "-o", "hello.o"};
    std::vector<std::string> expectedCommand = {"CC", "-c", "hello.c", "-o",
                                                "hello.o"};
    std::vector<std::string> expectedDepsCommand = {"CC", "-c", "hello.c",
                                                    "-xM"};
    std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_TRUE(command.produces_sun_make_rules());
}

TEST(XlcTest, NonCompilerCommands)
{
    EXPECT_FALSE(ParsedCommand({"xlc"}).is_compiler_command());
    EXPECT_FALSE(ParsedCommand({"xlc", "hello.c", "-o", "hello"})
                     .is_compiler_command());
    EXPECT_FALSE(
        ParsedCommand({"xlc", "hello.c", "-c", "-#"}).is_compiler_command());
}

TEST(XlcTest, OutputArguments)
{
    ParsedCommand command = {"xlc", "-c",      "hello.c",
                             "-o",  "hello.o", "-qexpfile=exportlist"};
    std::vector<std::string> expectedCommand = {
        "xlc", "-c", "hello.c", "-o", "hello.o", "-qexpfile=exportlist"};
    std::set<std::string> expectedProducts = {"hello.o", "exportlist"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_TRUE(command.produces_sun_make_rules());
}

TEST(RewriteAbsolutePathsTest, SimpleCompileCommand)
{
    RECC_PROJECT_ROOT = "/home/nobody/test/";
    const std::vector<std::string> command = {
        "gcc", "-c", "/home/nobody/test/hello.c", "-o",
        "/home/nobody/test/hello.o"};
    const ParsedCommand parsedCommand(command, "/home/nobody/test");

    const std::vector<std::string> expectedCommand = {"gcc", "-c", "hello.c",
                                                      "-o", "hello.o"};
    const std::vector<std::string> expectedDepsCommand = {"gcc", "-c",
                                                          "hello.c", "-M"};
    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
}

TEST(RewriteAbsolutePathsTest, ComplexOptions)
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
    const ParsedCommand parsedCommand(command, "/home/nobody/test");

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
        "hello.c",
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
        "/usr/include/something",
        "-M"};
    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
}
