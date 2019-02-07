#include <commandlineutils.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using BloombergLP::recc::CommandLineUtils;

using ::testing::ElementsAre;

TEST(StartsWith, StartsWithTests)
{
    EXPECT_TRUE(CommandLineUtils::starts_with("foobar", ""));
    EXPECT_TRUE(CommandLineUtils::starts_with("foobar", "foo"));
    EXPECT_TRUE(CommandLineUtils::starts_with("foobar", "foobar"));
    EXPECT_FALSE(CommandLineUtils::starts_with("foobar", "foobar "));
    EXPECT_FALSE(CommandLineUtils::starts_with("", "foobar"));
}

TEST(SplitOptionFromArg, NonOptions)
{
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("includefoo/bar"),
                ElementsAre("includefoo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg(""), ElementsAre(""));
}

TEST(SplitOptionFromArg, SupportedOptions)
{
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-includefoo/bar"),
                ElementsAre("-include", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-include/foo/bar"),
                ElementsAre("-include", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-imacros/foo/bar"),
                ElementsAre("-imacros", "/foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-imacros/foo/bar"),
                ElementsAre("-imacros", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-Ifoo/bar"),
                ElementsAre("-I", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-I/foo/bar"),
                ElementsAre("-I", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-iquotefoo/bar"),
                ElementsAre("-iquote", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-iquote/foo/bar"),
                ElementsAre("-iquote", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-isystemfoo/bar"),
                ElementsAre("-isystem", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-isystem/foo/bar"),
                ElementsAre("-isystem", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-idirafterfoo/bar"),
                ElementsAre("-idirafter", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-idirafter/foo/bar"),
                ElementsAre("-idirafter", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-iprefixfoo/bar"),
                ElementsAre("-iprefix", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-iprefix/foo/bar"),
                ElementsAre("-iprefix", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-isysrootfoo/bar"),
                ElementsAre("-isysroot", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-isysroot/foo/bar"),
                ElementsAre("-isysroot", "/foo/bar"));

    ASSERT_THAT(CommandLineUtils::split_option_from_arg("--sysroot=foo/bar"),
                ElementsAre("--sysroot=", "foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("--sysroot=/foo/bar"),
                ElementsAre("--sysroot=", "/foo/bar"));
}

TEST(SplitOptionFromArg, NothingToSplit)
{
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-isystem"),
                ElementsAre("-isystem"));
}

TEST(SplitOptionFromArg, UnsupportedOptions)
{
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-Lfoo/bar"),
                ElementsAre("-Lfoo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-iplugindir=foo/bar"),
                ElementsAre("-iplugindir=foo/bar"));
    ASSERT_THAT(CommandLineUtils::split_option_from_arg("-iplugindir=foo/bar"),
                ElementsAre("-iplugindir=foo/bar"));
}

TEST(PrependAbsolutePaths, BasicTests)
{
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"g++", "-Ifoo/bar", "baz.cpp"}, "/tmp/"),
                ElementsAre("g++", "-Ifoo/bar", "baz.cpp"));
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"g++", "-I/foo/bar", "baz.cpp"}, "/tmp/"),
                ElementsAre("g++", "-I/tmp//foo/bar", "baz.cpp"));
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"g++", "-I", "foo/bar", "baz.cpp"}, "/tmp/"),
                ElementsAre("g++", "-I", "foo/bar", "baz.cpp"));
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"g++", "-I", "/foo/bar", "baz.cpp"}, "/tmp/"),
                ElementsAre("g++", "-I", "/tmp//foo/bar", "baz.cpp"));
}

TEST(PrependAbsolutePaths, OutputPaths)
{
    // Not sure why anyone would do this but here's a test anyway
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"g++", "baz.cpp", "-o", "/lib/baz.o"}, "/tmp/"),
                ElementsAre("g++", "baz.cpp", "-o", "/tmp//lib/baz.o"));
}

TEST(PrependAbsolutePaths, SkipFirstArgument)
{
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"/usr/bin/g++", "-I/foo/bar", "baz.cpp"}, "/tmp/"),
                ElementsAre("/usr/bin/g++", "-I/tmp//foo/bar", "baz.cpp"));
}

TEST(PrependAbsolutePaths, EdgeCases)
{
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {}, "/tmp/"),
                ElementsAre());
    ASSERT_THAT(CommandLineUtils::prepend_absolute_paths_in_compile_command(
                    {"g++", "-I", "/foo/bar", "hello.cpp"}, ""),
                ElementsAre("g++", "-I", "/foo/bar", "hello.cpp"));
}
