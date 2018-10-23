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
#include <fileutils.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace std;

#ifdef RECC_PLATFORM_COMPILER

// Some compilers, like xlc, need certain environment variables to
// get the dependencies properly. So parse the env for these tests
TEST(DepsTest, Empty)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.", "empty.c"};
    set<string> expectedDeps = {"empty.c"};
    EXPECT_EQ(expectedDeps, get_file_info(command).dependencies);
}

TEST(DepsTest, SimpleInclude)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "includes_empty.c"};
    set<string> expected = {"includes_empty.c", "empty.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, RecursiveDependency)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "includes_includes_empty.c"};
    set<string> expected = {"includes_includes_empty.c", "includes_empty.h",
                            "empty.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, MultiFile)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "includes_includes_empty.c", "includes_empty.c"};
    set<string> expected = {"includes_includes_empty.c", "includes_empty.c",
                            "includes_empty.h", "empty.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, EdgeCases)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "edge_cases.c"};
    set<string> expected = {"edge_cases.c", "empty.h", "header with spaces.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, OutputArgument)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "includes_empty.c",     "-o", "/dev/null"};
    set<string> expected = {"includes_empty.c", "empty.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, OutputArgumentNoSpace)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "includes_empty.c", "-o/dev/null"};
    set<string> expected = {"includes_empty.c", "empty.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, PreprocessorOutputArgument)
{
    if (command_basename(RECC_PLATFORM_COMPILER) == "gcc") {
        ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                                 "includes_empty.c", "-Wp,-MMD,'/dev/null'"};
        set<string> expected = {"includes_empty.c", "empty.h"};
        EXPECT_EQ(expected, get_file_info(command).dependencies);
    }
}

TEST(DepsTest, Subdirectory)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-I.",
                             "-Isubdirectory", "includes_from_subdirectory.c"};
    set<string> expected = {"includes_from_subdirectory.c",
                            "subdirectory/header.h"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, InputInSubdirectory)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c",
                             "subdirectory/empty.c"};
    set<string> expected = {"subdirectory/empty.c"};
    EXPECT_EQ(expected, get_file_info(command).dependencies);
}

TEST(DepsTest, SubprocessFailure)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "empty.c",
                             "--clearly-invalid-option", "invalid_file.c"};
    EXPECT_THROW(get_file_info(command), subprocess_failed_error);
}

TEST(ProductsTest, OutputArgument)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-o",
                             "some_output.exe", "empty.c"};

    auto products = get_file_info(command).possibleProducts;

    EXPECT_EQ(1, products.count(string("some_output.exe")));
}

TEST(ProductsTest, NormalizesPath)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-o",
                             "out/subdir/../../../empty", "empty.c"};

    auto products = get_file_info(command).possibleProducts;

    EXPECT_EQ(1, products.count(string("../empty")));
}

TEST(ProductsTest, OutputArgumentNoSpace)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "-osome_output.exe",
                             "empty.c"};

    auto products = get_file_info(command).possibleProducts;

    EXPECT_EQ(1, products.count(string("some_output.exe")));
}

TEST(ProductsTest, DefaultOutput)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c", "empty.c"};

    auto products = get_file_info(command).possibleProducts;

    EXPECT_EQ(1, products.count(string("a.out")));
    EXPECT_EQ(1, products.count(string("empty.o")));
}

TEST(ProductsTest, Subdirectory)
{
    parse_config_variables();
    ParsedCommand command = {RECC_PLATFORM_COMPILER, "-c",
                             "subdirectory/empty.c"};

    auto products = get_file_info(command).possibleProducts;

    EXPECT_EQ(1, products.count("empty.o"));
    EXPECT_EQ(1, products.count("subdirectory/empty.c.gch"));
}

TEST(ProductsTest, PreprocessorArgument)
{
    if (command_basename(RECC_PLATFORM_COMPILER) == "gcc") {
        ParsedCommand command = {RECC_PLATFORM_COMPILER,   "-c", "empty.c",
                                 "-Wp,-MMD,'some/test.d'", "-o", "empty.o"};
        set<string> deps = {"empty.c"};

        auto products = get_file_info(command).possibleProducts;

        EXPECT_EQ(1, products.count("empty.o"));
        EXPECT_EQ(1, products.count("some/test.d"));
    }
}
#endif

TEST(DepsFromMakeRulesTest, GccStyleMakefile)
{
    string makeRules = "sample.o: sample.c sample.h /usr/include/cstring \\\n"
                       "   subdir/sample.h\n"
                       "rule2.o: sample.h";
    set<string> expected = {"sample.c", "sample.h", "subdir/sample.h"};

    auto dependencies = dependencies_from_make_rules(makeRules);

    EXPECT_EQ(expected, dependencies);
}

TEST(DepsFromMakeRulesTest, SunStyleMakefile)
{
    string makeRules = "sample.o : ./sample.c\n"
                       "sample.o : ./sample.h\n"
                       "sample.o : /usr/include/cstring\n"
                       "sample.o : ./subdir/sample.h\n"
                       "rule2.o : ./sample.h\n"
                       "rule3.o : ./sample with spaces.c";
    set<string> expected = {"sample.c", "sample.h", "subdir/sample.h",
                            "sample with spaces.c"};

    auto dependencies = dependencies_from_make_rules(makeRules, true);

    EXPECT_EQ(expected, dependencies);
}

TEST(DepsFromMakeRulesTest, LargeMakeOutput)
{
    auto makeRules = get_file_contents("giant_make_output.mk");
    set<string> expected = {"hello.c", "hello.h", "final_dependency.h"};

    auto dependencies = dependencies_from_make_rules(makeRules);

    EXPECT_EQ(expected, dependencies);
}
