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

#include <deps.h>
#include <env.h>
#include <fileutils.h>
#include <parsedcommand.h>

#include <buildboxcommon_fileutils.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;

std::set<std::string> normalize_all(const std::set<std::string> &paths)
{
    std::set<std::string> result;
    for (const auto &path : paths) {
        result.insert(buildboxcommon::FileUtils::normalizePath(path.c_str()));
    }
    return result;
}

// Set in the top-level CMakeLists.txt depending on the platform.
#ifdef RECC_PLATFORM_COMPILER

// Some compilers, like xlc, need certain environment variables to
// get the dependencies properly. So parse the env for these tests
TEST(DepsTest, Empty)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "empty.c"});
    std::set<std::string> expectedDeps = {"empty.c"};
    EXPECT_EQ(expectedDeps,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, SimpleInclude)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "includes_empty.c"});
    std::set<std::string> expected = {"includes_empty.c", "empty.h"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, RecursiveDependency)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "includes_includes_empty.c"});
    std::set<std::string> expected = {"includes_includes_empty.c",
                                      "includes_empty.h", "empty.h"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, MultiFile)
{
    // Exclude this test on AIX, as the compiler doesn't support writing
    // multiple source files dependency information to the same file without
    // overriding the contents.
    if (RECC_PLATFORM_COMPILER != "xlc") {
        Env::parse_config_variables();
        RECC_DEPS_GLOBAL_PATHS = 0;
        const auto command = ParsedCommandFactory::createParsedCommand(
            {RECC_PLATFORM_COMPILER, "-c", "-I.", "includes_includes_empty.c",
             "includes_empty.c"});
        std::set<std::string> expected = {"includes_includes_empty.c",
                                          "includes_empty.c",
                                          "includes_empty.h", "empty.h"};
        EXPECT_EQ(expected,
                  normalize_all(Deps::get_file_info(command).d_dependencies));
    }
}

TEST(DepsTest, EdgeCases)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "edge_cases.c"});
    std::set<std::string> expected = {"edge_cases.c", "empty.h",
                                      "header with spaces.h"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, OutputArgument)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "includes_empty.c", "-o",
         "/dev/null"});
    std::set<std::string> expected = {"includes_empty.c", "empty.h"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, OutputArgumentNoSpace)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "includes_empty.c",
         "-o/dev/null"});
    std::set<std::string> expected = {"includes_empty.c", "empty.h"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, PreprocessorOutputArgument)
{
    if (ParsedCommand::commandBasename(RECC_PLATFORM_COMPILER) == "gcc") {
        const auto command = ParsedCommandFactory::createParsedCommand(
            {RECC_PLATFORM_COMPILER, "-c", "-I.", "includes_empty.c",
             "-Wp,-MMD,'/dev/null'"});
        std::set<std::string> expected = {"includes_empty.c", "empty.h"};
        EXPECT_EQ(expected,
                  normalize_all(Deps::get_file_info(command).d_dependencies));
    }
}

TEST(DepsTest, Subdirectory)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-I.", "-Isubdirectory",
         "includes_from_subdirectory.c"});
    std::set<std::string> expected = {"includes_from_subdirectory.c",
                                      "subdirectory/header.h"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, SystemSubdirectory)
{
    if (ParsedCommand::commandBasename(RECC_PLATFORM_COMPILER) == "gcc") {
        Env::parse_config_variables();
        RECC_DEPS_GLOBAL_PATHS = 0;
        const auto command = ParsedCommandFactory::createParsedCommand(
            {RECC_PLATFORM_COMPILER, "-c", "-I.", "-isystemsubdirectory",
             "includes_from_subdirectory.c"});
        std::set<std::string> expected = {"includes_from_subdirectory.c",
                                          "subdirectory/header.h"};
        EXPECT_EQ(expected,
                  normalize_all(Deps::get_file_info(command).d_dependencies));
    }
}

TEST(DepsTest, InputInSubdirectory)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "subdirectory/empty.c"});
    std::set<std::string> expected = {"subdirectory/empty.c"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, SubprocessFailure)
{
    Env::parse_config_variables();
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "empty.c", "--clearly-invalid-option",
         "invalid_file.c"});
    EXPECT_THROW(Deps::get_file_info(command), subprocess_failed_error);
}

TEST(DepsTest, GlobalPathsAllowed)
{
    Env::parse_config_variables();
    RECC_DEPS_GLOBAL_PATHS = 0;
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "ctype_include.c"});
    std::set<std::string> expected = {"ctype_include.c"};
    EXPECT_EQ(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));

    RECC_DEPS_GLOBAL_PATHS = 1;
    EXPECT_NE(expected,
              normalize_all(Deps::get_file_info(command).d_dependencies));
}

TEST(DepsTest, ClangCrtbegin)
{
    // clang-format off
    std::string clang_v_common =
        "clang version 9.0.0 (https://github.com/llvm/llvm-project/ 67510fac36d27b2e22c7cd955fc167136b737b93)\n"
        "Target: x86_64-unknown-linux-gnu\n"
        "Thread model: posix\n"
        "InstalledDir: /home/user/clang/bin\n"
        "Found candidate GCC installation: /usr/lib/gcc/i686-linux-gnu/5\n"
        "Found candidate GCC installation: /usr/lib/gcc/i686-linux-gnu/5.4.0\n"
        "Found candidate GCC installation: /usr/lib/gcc/i686-linux-gnu/6\n"
        "Found candidate GCC installation: /usr/lib/gcc/i686-linux-gnu/6.0.0\n"
        "Found candidate GCC installation: /usr/lib/gcc/x86_64-linux-gnu/5\n"
        "Found candidate GCC installation: /usr/lib/gcc/x86_64-linux-gnu/5.4.0\n"
        "Found candidate GCC installation: /usr/lib/gcc/x86_64-linux-gnu/6\n"
        "Found candidate GCC installation: /usr/lib/gcc/x86_64-linux-gnu/6.0.0\n"
        "Selected GCC installation: /usr/lib/gcc/x86_64-linux-gnu/5.4.0\n"
        "Candidate multilib: .;@m64\n"
        "Candidate multilib: 32;@m32\n"
        "Candidate multilib: x32;@mx32\n";
    // clang-format on

    std::string clang_v_dot = clang_v_common + "Selected multilib: .;@m64\n";
    std::string clang_v_foo = clang_v_common + "Selected multilib: foo;@m64\n";

    std::string expected_dot =
        "/usr/lib/gcc/x86_64-linux-gnu/5.4.0/crtbegin.o";
    std::string found = Deps::crtbegin_from_clang_v(clang_v_dot);
    EXPECT_EQ(expected_dot, found);

    std::string expected_foo =
        "/usr/lib/gcc/x86_64-linux-gnu/5.4.0/foo/crtbegin.o";
    found = Deps::crtbegin_from_clang_v(clang_v_foo);
    EXPECT_EQ(expected_foo, found);
}

TEST(ProductsTest, OutputArgument)
{
    Env::parse_config_variables();
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-o", "some_output.exe", "empty.c"});

    auto products = Deps::get_file_info(command).d_possibleProducts;

    EXPECT_EQ(1, products.count(std::string("some_output.exe")));
}

TEST(ProductsTest, NormalizesPath)
{
    Env::parse_config_variables();
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-o", "out/subdir/../../../empty",
         "empty.c"});

    auto products = Deps::get_file_info(command).d_possibleProducts;

    EXPECT_EQ(1, products.count(std::string("../empty")));
}

TEST(ProductsTest, OutputArgumentNoSpace)
{
    Env::parse_config_variables();
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "-osome_output.exe", "empty.c"});

    auto products = Deps::get_file_info(command).d_possibleProducts;

    EXPECT_EQ(1, products.count(std::string("some_output.exe")));
}

TEST(ProductsTest, DefaultOutput)
{
    Env::parse_config_variables();
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "empty.c"});

    auto products = Deps::get_file_info(command).d_possibleProducts;

    EXPECT_EQ(1, products.count(std::string("a.out")));
    EXPECT_EQ(1, products.count(std::string("empty.o")));
}

TEST(ProductsTest, Subdirectory)
{
    Env::parse_config_variables();
    const auto command = ParsedCommandFactory::createParsedCommand(
        {RECC_PLATFORM_COMPILER, "-c", "subdirectory/empty.c"});

    auto products = Deps::get_file_info(command).d_possibleProducts;

    EXPECT_EQ(1, products.count("empty.o"));
    EXPECT_EQ(1, products.count("subdirectory/empty.c.gch"));
}

TEST(ProductsTest, PreprocessorArgument)
{
    if (ParsedCommand::commandBasename(RECC_PLATFORM_COMPILER) == "gcc") {
        const auto command = ParsedCommandFactory::createParsedCommand(
            {RECC_PLATFORM_COMPILER, "-c", "empty.c", "-Wp,-MMD,'some/test.d'",
             "-o", "empty.o"});
        std::set<std::string> deps = {"empty.c"};

        auto products = Deps::get_file_info(command).d_possibleProducts;

        EXPECT_EQ(1, products.count("empty.o"));
        EXPECT_EQ(1, products.count("some/test.d"));
    }
}
#endif

TEST(DepsFromMakeRulesTest, GccStyleMakefile)
{
    std::string makeRules =
        "sample.o: sample.c sample.h /usr/include/cstring \\\n"
        "   subdir/sample.h\n"
        "rule2.o: sample.h";
    std::set<std::string> expected = {"sample.c", "sample.h",
                                      "subdir/sample.h"};

    auto dependencies =
        normalize_all(Deps::dependencies_from_make_rules(makeRules));

    EXPECT_EQ(expected, dependencies);
}

TEST(DepsFromMakeRulesTest, SunStyleMakefile)
{
    std::string makeRules = "sample.o : ./sample.c\n"
                            "sample.o : ./sample.h\n"
                            "sample.o : /usr/include/cstring\n"
                            "sample.o : ./subdir/sample.h\n"
                            "rule2.o : ./sample.h\n"
                            "rule3.o : ./sample with spaces.c";
    std::set<std::string> expected = {
        "sample.c", "sample.h", "subdir/sample.h", "sample with spaces.c"};

    auto dependencies =
        normalize_all(Deps::dependencies_from_make_rules(makeRules, true));

    EXPECT_EQ(expected, dependencies);
}

TEST(DepsFromMakeRulesTest, LargeMakeOutput)
{
    auto makeRules =
        buildboxcommon::FileUtils::getFileContents("giant_make_output.mk");
    std::set<std::string> expected = {"hello.c", "hello.h",
                                      "final_dependency.h"};

    auto dependencies =
        normalize_all(Deps::dependencies_from_make_rules(makeRules));

    EXPECT_EQ(expected, dependencies);
}
