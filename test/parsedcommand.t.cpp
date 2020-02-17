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

TEST(CommandBasenameTest, EmptyString)
{
    EXPECT_EQ(ParsedCommand::command_basename(""), "");
}

TEST(CommandBasenameTest, TrivialCommands)
{
    EXPECT_EQ(ParsedCommand::command_basename("gcc"), "gcc");
    EXPECT_EQ(ParsedCommand::command_basename("g++"), "g++");
    EXPECT_EQ(ParsedCommand::command_basename("CC"), "CC");
    EXPECT_EQ(ParsedCommand::command_basename("clang"), "clang");
    EXPECT_EQ(ParsedCommand::command_basename("clang++"), "clang++");
}

TEST(CommandBasenameTest, CommandsWithVersions)
{
    EXPECT_EQ(ParsedCommand::command_basename("gcc-4.7"), "gcc");
    EXPECT_EQ(ParsedCommand::command_basename("CC++-99"), "CC++");
    EXPECT_EQ(ParsedCommand::command_basename("clang-6.0"), "clang");
    EXPECT_EQ(ParsedCommand::command_basename("clang++-6.0"), "clang++");
}

TEST(CommandBasenameTest, CommandsAtPaths)
{
    EXPECT_EQ(ParsedCommand::command_basename("/usr/bin/gcc"), "gcc");
    EXPECT_EQ(ParsedCommand::command_basename("/usr/bin/g++"), "g++");
    EXPECT_EQ(ParsedCommand::command_basename("/CC++-99"), "CC++");
    EXPECT_EQ(ParsedCommand::command_basename("/usr/bin/clang"), "clang");
    EXPECT_EQ(ParsedCommand::command_basename("/usr/bin/clang++"), "clang++");
}

TEST(CommandBasenameTest, XlcVersions)
{
    EXPECT_EQ(ParsedCommand::command_basename("xlC128_r"), "xlC");
    EXPECT_EQ(ParsedCommand::command_basename("xlc++_r7"), "xlc++");
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

    command = {"/usr/local/bin/clang", "-c", "test/file.cpp"};
    EXPECT_TRUE(command.is_compiler_command());
}

TEST(IsCompilerTest, NotCompiler)
{
    ParsedCommand command = {"cat", "something.c"};
    EXPECT_FALSE(command.is_compiler_command());

    command = {"clang-format", "something.c"};
    EXPECT_FALSE(command.is_compiler_command());
}

TEST(IsCompilerTest, EmptyString)
{
    ParsedCommand command = {""};
    EXPECT_FALSE(command.is_compiler_command());
}

void nonCompilerCommands(const char *gcc_or_clang)
{
    EXPECT_FALSE(ParsedCommand({gcc_or_clang}).is_compiler_command());
    EXPECT_FALSE(
        ParsedCommand({gcc_or_clang, "-version"}).is_compiler_command());
    EXPECT_FALSE(
        ParsedCommand({gcc_or_clang, "-dumpmachine"}).is_compiler_command());
    EXPECT_FALSE(ParsedCommand({gcc_or_clang, "hello.c", "-o", "hello"})
                     .is_compiler_command());
}

TEST(GccClangTest, NonCompilerCommands)
{
    nonCompilerCommands("gcc");
    nonCompilerCommands("clang");
}

void simpleCommand(const char *gcc_or_clang)
{
    ParsedCommand command = {gcc_or_clang, "-c", "hello.c"};
    std::vector<std::string> expectedCommand = {gcc_or_clang, "-c", "hello.c"};
    std::vector<std::string> expectedDepsCommand = {gcc_or_clang, "-c",
                                                    "hello.c", "-M"};
    if (RECC_DEPS_GLOBAL_PATHS && command.is_clang()) {
        expectedDepsCommand.push_back("-v");
    }
    std::set<std::string> expectedProducts = {};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccClangTest, SimpleCommand)
{
    simpleCommand("gcc");
    simpleCommand("clang");
}

void outputArgument(const char *gcc_or_clang)
{
    ParsedCommand command = {gcc_or_clang, "-c", "hello.c", "-o", "hello.o"};
    std::vector<std::string> expectedCommand = {gcc_or_clang, "-c", "hello.c",
                                                "-o", "hello.o"};
    std::vector<std::string> expectedDepsCommand = {gcc_or_clang, "-c",
                                                    "hello.c", "-M"};
    if (RECC_DEPS_GLOBAL_PATHS && command.is_clang()) {
        expectedDepsCommand.push_back("-v");
    }
    std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccClangTest, OutputArgument)
{
    outputArgument("gcc");
    outputArgument("clang");
}

void outputArgumentNoSpace(const char *gcc_or_clang)
{
    ParsedCommand command = {gcc_or_clang, "-c", "-ohello.o", "hello.c"};
    std::vector<std::string> expectedCommand = {gcc_or_clang, "-c",
                                                "-ohello.o", "hello.c"};
    std::vector<std::string> expectedDepsCommand = {gcc_or_clang, "-c",
                                                    "hello.c", "-M"};
    if (RECC_DEPS_GLOBAL_PATHS && command.is_clang()) {
        expectedDepsCommand.push_back("-v");
    }
    std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_command(), expectedCommand);
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccClangTest, OutputArgumentNoSpace)
{
    outputArgumentNoSpace("gcc");
    outputArgumentNoSpace("clang");
}

void preprocessorArguments(const char *gcc_or_clang)
{
    ParsedCommand command = {gcc_or_clang, "-c",      "-Xpreprocessor",
                             "-MMD",       "hello.c", "-Wp,hello.d,-MV,-valid",
                             "-ohello.o"};
    std::vector<std::string> expectedDepsCommand = {
        gcc_or_clang, "-c", "hello.c", "-Xpreprocessor", "-valid", "-M"};
    if (RECC_DEPS_GLOBAL_PATHS && command.is_clang()) {
        expectedDepsCommand.push_back("-v");
    }
    std::set<std::string> expectedProducts = {"hello.o", "hello.d"};

    ASSERT_TRUE(command.is_compiler_command());
    EXPECT_EQ(command.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(command.get_products(), expectedProducts);
    EXPECT_FALSE(command.produces_sun_make_rules());
}

TEST(GccClangTest, PreprocessorArguments)
{
    preprocessorArguments("gcc");
    preprocessorArguments("clang");
}

TEST(GccClangTest, RecogniseClang)
{
    EXPECT_FALSE(ParsedCommand({"gcc"}).is_clang());
    EXPECT_FALSE(ParsedCommand({"g++"}).is_clang());

    EXPECT_TRUE(ParsedCommand({"clang"}).is_clang());
    EXPECT_TRUE(ParsedCommand({"clang++"}).is_clang());
    EXPECT_TRUE(ParsedCommand({"clang-6.0"}).is_clang());
    EXPECT_TRUE(ParsedCommand({"clang++-6.0"}).is_clang());
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

TEST(ReplacePathTest, SimpleRewrite)
{
    RECC_PREFIX_REPLACEMENT = {{"/usr/bin/include", "/include"}};
    RECC_PROJECT_ROOT = "/home/nobody/";

    const std::vector<std::string> command = {
        "gcc", "-c", "hello.c", "-I/usr/bin/include/headers", "-o", "hello.o"};

    const ParsedCommand parsedCommand(command, "/home");

    // -I should be replaced.
    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "hello.c", "-I/include/headers", "-o", "hello.o"};

    // Deps command shouldn't be rewritten.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "hello.c", "-I/usr/bin/include/headers", "-M"};

    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
}

TEST(ReplacePathTest, PathInProjectRoot)
{
    // Path to replace is inside project root, the behavior expected is:
    // Path replaced by path in PREFIX_MAP, then if still relative to
    // PROJECT_ROOT Replaced again to be made relative.
    RECC_PREFIX_REPLACEMENT = {{"/home/usr/bin", "/home/bin"}};
    RECC_PROJECT_ROOT = "/home/";

    const std::vector<std::string> command = {
        "gcc", "-c",     "hello.c", "-I/home/usr/bin/include/headers",
        "-o",  "hello.o"};

    const ParsedCommand parsedCommand(command, "/home/");

    // Deps command shouldn't be rewritten.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "hello.c", "-Iusr/bin/include/headers", "-M"};

    // -I should be replaced, and made relative
    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "hello.c", "-Ibin/include/headers", "-o", "hello.o"};

    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
}

TEST(ReplacePathTest, SimpleCompilePathReplacement)
{
    RECC_PREFIX_REPLACEMENT = {{"/home/usr/bin", "/home/bin"}};
    const std::vector<std::string> command = {"gcc", "-c",
                                              "/home/usr/bin/hello.c"};
    const ParsedCommand parsedCommand(command, "");

    // Deps command shouldn't be rewritten.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "/home/usr/bin/hello.c", "-M"};

    const std::vector<std::string> expectedCommand = {"gcc", "-c",
                                                      "/home/bin/hello.c"};
    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
}

TEST(ReplacePathTest, ReplaceCompilePathInProjectRoot)
{
    // Path to replace is inside project root, the behavior expected is:
    // Path replaced by path in PREFIX_MAP, then if still relative to
    // PROJECT_ROOT Replaced again to be made relative.
    RECC_PREFIX_REPLACEMENT = {{"/home/usr/bin", "/home/bin"}};
    RECC_PROJECT_ROOT = "/home/";

    const std::vector<std::string> command = {
        "gcc",
        "-c",
        "/home/usr/bin/hello.c",
        "-I/home/usr/bin/include/headers",
        "-o",
        "hello.o"};

    const ParsedCommand parsedCommand(command, "/home/");

    // Deps command shouldn't be rewritten.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "usr/bin/hello.c", "-Iusr/bin/include/headers", "-M"};

    // -I should be replaced, and made relative
    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "bin/hello.c", "-Ibin/include/headers", "-o", "hello.o"};

    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
}
