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

#include <fileutils.h>

#include <env.h>
#include <subprocess.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <exception>
#include <sys/stat.h>

using namespace BloombergLP::recc;

TEST(FileUtilsTest, TemporaryDirectory)
{
    std::string name;
    {
        TemporaryDirectory tempDir("test-prefix");
        name = std::string(tempDir.name());
        EXPECT_NE(name.find("test-prefix"), std::string::npos);

        // Verify that the directory exists and is a directory.
        struct stat statResult;
        ASSERT_EQ(stat(tempDir.name(), &statResult), 0);
        ASSERT_TRUE(S_ISDIR(statResult.st_mode));
    }

    // Verify that the directory no longer exists.
    struct stat statResult;
    ASSERT_NE(stat(name.c_str(), &statResult), 0);
}

TEST(FileUtilsTest, CreateDirectoryRecursive)
{
    TemporaryDirectory tempDir;
    std::string name = tempDir.name() + std::string("/some/directory/path");

    create_directory_recursive(name.c_str());

    struct stat statResult;
    ASSERT_EQ(stat(name.c_str(), &statResult), 0);
    ASSERT_TRUE(S_ISDIR(statResult.st_mode));
}

TEST(FileUtilsTest, Executable)
{
    TemporaryDirectory tempDir;
    std::string fileName = tempDir.name() + std::string("/testfile");

    ASSERT_THROW(is_executable(fileName.c_str()), std::exception);
    ASSERT_THROW(make_executable(fileName.c_str()), std::exception);

    write_file(fileName.c_str(), ""); // Create a test file.

    EXPECT_FALSE(is_executable(fileName.c_str()));
    make_executable(fileName.c_str());
    EXPECT_TRUE(is_executable(fileName.c_str()));
}

TEST(FileUtilsTest, FileContents)
{
    TemporaryDirectory tempDir;
    std::string fileName = tempDir.name() + std::string("/testfile");

    EXPECT_THROW(get_file_contents(fileName.c_str()), std::exception);

    write_file(fileName.c_str(), "File contents");
    EXPECT_EQ(get_file_contents(fileName.c_str()), "File contents");

    write_file(fileName.c_str(), "Overwrite, don't append");
    EXPECT_EQ(get_file_contents(fileName.c_str()), "Overwrite, don't append");
}

TEST(FileUtilsTest, FileContentsCreatesDirectory)
{
    TemporaryDirectory tempDir;
    std::string fileName =
        tempDir.name() + std::string("/some/subdirectory/file.txt");

    write_file(fileName.c_str(), "File contents");
    EXPECT_EQ(get_file_contents(fileName.c_str()), "File contents");
}

TEST(NormalizePathTest, AlreadyNormalPaths)
{
    EXPECT_EQ("test.txt", normalize_path("test.txt"));
    EXPECT_EQ("subdir/hello", normalize_path("subdir/hello"));
    EXPECT_EQ("/usr/bin/gcc", normalize_path("/usr/bin/gcc"));
}

TEST(NormalizePathTest, RemoveEmptySegments)
{
    EXPECT_EQ("subdir/hello", normalize_path("subdir///hello//"));
    EXPECT_EQ("/usr/bin/gcc", normalize_path("/usr/bin/./gcc"));
}

TEST(NormalizePathTest, RemoveUnneededDotDot)
{
    EXPECT_EQ("subdir/hello", normalize_path("subdir/subsubdir/../hello"));
    EXPECT_EQ("/usr/bin/gcc",
              normalize_path("/usr/local/lib/../../bin/.//gcc"));
}

TEST(NormalizePathTest, KeepNeededDotDot)
{
    EXPECT_EQ("../dir/hello", normalize_path("../dir/hello"));
    EXPECT_EQ("../dir/hello", normalize_path("subdir/../../dir/hello"));
    EXPECT_EQ("../../dir/hello", normalize_path("subdir/../../../dir/hello"));
}

TEST(NormalizePathTest, AlwaysRemoveTrailingSlash)
{
    EXPECT_EQ("/usr/bin", normalize_path("/usr/bin"));
    EXPECT_EQ("/usr/bin", normalize_path("/usr/bin/"));
}

TEST(HasPathPrefixTest, AbsolutePaths)
{
    EXPECT_TRUE(has_path_prefix("/a/b/c/", "/a/b"));
    EXPECT_TRUE(has_path_prefix("/a/b/c/", "/a/b/"));
    EXPECT_TRUE(has_path_prefix("/a/b/c", "/a/b"));
    EXPECT_TRUE(has_path_prefix("/a/b/c", "/a/b/"));

    EXPECT_FALSE(has_path_prefix("/a/c/d", "/a/b/"));

    EXPECT_FALSE(has_path_prefix("/a/boo", "/a/b/"));
    EXPECT_FALSE(has_path_prefix("/a/boo", "/a/b"));
    EXPECT_FALSE(has_path_prefix("/a/boo", "/a/b/a/boo"));
    EXPECT_FALSE(has_path_prefix("/a/boo", "/a/b/a/boo/"));

    EXPECT_TRUE(has_path_prefix("/a/../b/", "/a"));
    EXPECT_TRUE(has_path_prefix("/a/../b/", "/a/"));
    EXPECT_TRUE(has_path_prefix("/a/../b", "/a"));
    EXPECT_TRUE(has_path_prefix("/a/../b", "/a/"));
}

TEST(HasPathPrefixTest, RelativePaths)
{
    EXPECT_TRUE(has_path_prefix("a/b/c/", "a/b"));
    EXPECT_TRUE(has_path_prefix("a/b/c/", "a/b/"));
    EXPECT_TRUE(has_path_prefix("a/b/c", "a/b"));
    EXPECT_TRUE(has_path_prefix("a/b/c", "a/b/"));

    EXPECT_FALSE(has_path_prefix("a/c/d", "a/b/"));

    EXPECT_FALSE(has_path_prefix("a/boo", "a/b/"));
    EXPECT_FALSE(has_path_prefix("a/boo", "a/b"));
    EXPECT_FALSE(has_path_prefix("a/boo", "a/b/a/boo"));
    EXPECT_FALSE(has_path_prefix("a/boo", "a/b/a/boo/"));

    EXPECT_TRUE(has_path_prefix("a/../b/", "a"));
    EXPECT_TRUE(has_path_prefix("a/../b/", "a/"));
    EXPECT_TRUE(has_path_prefix("a/../b", "a"));
    EXPECT_TRUE(has_path_prefix("a/../b", "a/"));

    EXPECT_FALSE(has_path_prefix("/a/b/c/", "a/b/"));
    EXPECT_FALSE(has_path_prefix("/a/b/c/", "a/b"));
    EXPECT_FALSE(has_path_prefix("/a/b/c", "a/b/"));
    EXPECT_FALSE(has_path_prefix("/a/b/c", "a/b"));
}

class MakePathRelativeTest : public ::testing::Test {
  protected:
    void SetUp() override { RECC_PROJECT_ROOT = "/"; }
};

TEST_F(MakePathRelativeTest, ReturnNonAbsolutePathsUnmodified)
{
    EXPECT_EQ("",
              make_path_relative(std::string(""), "/some/working/directory"));
    EXPECT_EQ("",
              make_path_relative(std::string(""), "/some/working/directory"));
    EXPECT_EQ("test", make_path_relative(std::string("test"),
                                         "/some/working/directory"));
    EXPECT_EQ("test/long/path",
              make_path_relative(std::string("test/long/path"),
                                 "/some/working/directory"));
    EXPECT_EQ("some/path", make_path_relative(std::string("some/path"),
                                              "/some/working/directory"));
}

TEST_F(MakePathRelativeTest, DoNothingIfWorkingDirectoryNull)
{
    EXPECT_EQ("/test/directory/",
              make_path_relative(std::string("/test/directory/"), nullptr));
    EXPECT_EQ("/test/directory/",
              make_path_relative(std::string("/test/directory/"), ""));
    EXPECT_EQ("/test", make_path_relative(std::string("/test"), ""));
}

TEST_F(MakePathRelativeTest, WorkingDirectoryIsPathPrefix)
{
    EXPECT_EQ("path", make_path_relative(std::string("/some/test/path"),
                                         "/some/test"));
    EXPECT_EQ("path", make_path_relative(std::string("/some/test/path"),
                                         "/some/test/"));
    EXPECT_EQ("path/", make_path_relative(std::string("/some/test/path/"),
                                          "/some/test"));
    EXPECT_EQ("path/", make_path_relative(std::string("/some/test/path/"),
                                          "/some/test/"));
}

TEST_F(MakePathRelativeTest, PathEqualsWorkingDirectory)
{
    EXPECT_EQ(".", make_path_relative(std::string("/some/test/path"),
                                      "/some/test/path"));
    EXPECT_EQ(".", make_path_relative(std::string("/some/test/path"),
                                      "/some/test/path/"));
    EXPECT_EQ("./", make_path_relative(std::string("/some/test/path/"),
                                       "/some/test/path"));
    EXPECT_EQ("./", make_path_relative(std::string("/some/test/path/"),
                                       "/some/test/path/"));
}

TEST_F(MakePathRelativeTest, PathAlmostEqualsWorkingDirectory)
{
    EXPECT_EQ("../tests",
              make_path_relative(std::string("/some/tests"), "/some/test"));
    EXPECT_EQ("../tests",
              make_path_relative(std::string("/some/tests"), "/some/test/"));
    EXPECT_EQ("../tests/",
              make_path_relative(std::string("/some/tests/"), "/some/test"));
    EXPECT_EQ("../tests/",
              make_path_relative(std::string("/some/tests/"), "/some/test/"));
}

TEST_F(MakePathRelativeTest, PathIsParentOfWorkingDirectory)
{
    EXPECT_EQ("..", make_path_relative(std::string("/a/b/c"), "/a/b/c/d"));
    EXPECT_EQ("..", make_path_relative(std::string("/a/b/c"), "/a/b/c/d/"));
    EXPECT_EQ("../", make_path_relative(std::string("/a/b/c/"), "/a/b/c/d"));
    EXPECT_EQ("../", make_path_relative(std::string("/a/b/c/"), "/a/b/c/d/"));

    EXPECT_EQ("../../..", make_path_relative(std::string("/a"), "/a/b/c/d"));
    EXPECT_EQ("../../..", make_path_relative(std::string("/a"), "/a/b/c/d/"));
    EXPECT_EQ("../../../", make_path_relative(std::string("/a/"), "/a/b/c/d"));
    EXPECT_EQ("../../../",
              make_path_relative(std::string("/a/"), "/a/b/c/d/"));
}

TEST_F(MakePathRelativeTest, PathAdjacentToWorkingDirectory)
{
    EXPECT_EQ("../e", make_path_relative(std::string("/a/b/c/e"), "/a/b/c/d"));
    EXPECT_EQ("../e",
              make_path_relative(std::string("/a/b/c/e"), "/a/b/c/d/"));
    EXPECT_EQ("../e/",
              make_path_relative(std::string("/a/b/c/e/"), "/a/b/c/d"));
    EXPECT_EQ("../e/",
              make_path_relative(std::string("/a/b/c/e/"), "/a/b/c/d/"));

    EXPECT_EQ("../e/f/g",
              make_path_relative(std::string("/a/b/c/e/f/g"), "/a/b/c/d"));
    EXPECT_EQ("../e/f/g",
              make_path_relative(std::string("/a/b/c/e/f/g"), "/a/b/c/d/"));
    EXPECT_EQ("../e/f/g/",
              make_path_relative(std::string("/a/b/c/e/f/g/"), "/a/b/c/d"));
    EXPECT_EQ("../e/f/g/",
              make_path_relative(std::string("/a/b/c/e/f/g/"), "/a/b/c/d/"));

    EXPECT_EQ("../../e/f/g",
              make_path_relative(std::string("/a/b/e/f/g"), "/a/b/c/d"));
    EXPECT_EQ("../../e/f/g",
              make_path_relative(std::string("/a/b/e/f/g"), "/a/b/c/d/"));
    EXPECT_EQ("../../e/f/g/",
              make_path_relative(std::string("/a/b/e/f/g/"), "/a/b/c/d"));
    EXPECT_EQ("../../e/f/g/",
              make_path_relative(std::string("/a/b/e/f/g/"), "/a/b/c/d/"));
}

TEST_F(MakePathRelativeTest, PathOutsideProjectRoot)
{
    RECC_PROJECT_ROOT = "/home/nobody/test/";
    EXPECT_EQ("/home/nobody/include/foo.h",
              make_path_relative(std::string("/home/nobody/include/foo.h"),
                                 "/home/nobody/test"));
    EXPECT_EQ("/home/nobody/include/foo.h",
              make_path_relative(std::string("/home/nobody/include/foo.h"),
                                 "/home/nobody/test"));
}

TEST(MakePathAbsoluteTest, NoRewriting)
{
    EXPECT_EQ("", make_path_absolute("", "/a/b/c/"));
    EXPECT_EQ("/a/b/c/", make_path_absolute("/a/b/c/", "/d/"));
}

TEST(MakePathAbsoluteTest, SimplePaths)
{
    EXPECT_EQ("/a/b/c/d", make_path_absolute("d", "/a/b/c/"));
    EXPECT_EQ("/a/b/c/d/", make_path_absolute("d/", "/a/b/c/"));
    EXPECT_EQ("/a/b", make_path_absolute("..", "/a/b/c/"));
    EXPECT_EQ("/a/b/", make_path_absolute("../", "/a/b/c/"));
    EXPECT_EQ("/a/b", make_path_absolute("..", "/a/b/c"));
    EXPECT_EQ("/a/b/", make_path_absolute("../", "/a/b/c"));

    EXPECT_EQ("/a/b/c", make_path_absolute(".", "/a/b/c/"));
    EXPECT_EQ("/a/b/c/", make_path_absolute("./", "/a/b/c/"));
    EXPECT_EQ("/a/b/c", make_path_absolute(".", "/a/b/c"));
    EXPECT_EQ("/a/b/c/", make_path_absolute("./", "/a/b/c"));
}

TEST(MakePathAbsoluteTest, MoreComplexPaths)
{
    EXPECT_EQ("/a/b/d", make_path_absolute("../d", "/a/b/c"));
    EXPECT_EQ("/a/b/d", make_path_absolute("../d", "/a/b/c/"));
    EXPECT_EQ("/a/b/d/", make_path_absolute("../d/", "/a/b/c"));
    EXPECT_EQ("/a/b/d/", make_path_absolute("../d/", "/a/b/c/"));

    EXPECT_EQ("/a/b/d", make_path_absolute("./.././d", "/a/b/c"));
    EXPECT_EQ("/a/b/d", make_path_absolute("./.././d", "/a/b/c/"));
    EXPECT_EQ("/a/b/d/", make_path_absolute("./.././d/", "/a/b/c"));
    EXPECT_EQ("/a/b/d/", make_path_absolute("./.././d/", "/a/b/c/"));
}

TEST(FileUtilsTest, GetCurrentWorkingDirectory)
{
    const std::vector<std::string> command = {"pwd"};
    const auto commandResult = execute(command, true);
    if (commandResult.d_exitCode == 0) {
        EXPECT_EQ(commandResult.d_stdOut,
                  get_current_working_directory() + "\n");
    }
}

TEST(FileUtilsTest, ParentDirectoryLevels)
{
    EXPECT_EQ(parent_directory_levels(""), 0);
    EXPECT_EQ(parent_directory_levels("/"), 0);
    EXPECT_EQ(parent_directory_levels("."), 0);
    EXPECT_EQ(parent_directory_levels("./"), 0);

    EXPECT_EQ(parent_directory_levels(".."), 1);
    EXPECT_EQ(parent_directory_levels("../"), 1);
    EXPECT_EQ(parent_directory_levels("../.."), 2);
    EXPECT_EQ(parent_directory_levels("../../"), 2);

    EXPECT_EQ(parent_directory_levels("a/b/c.txt"), 0);
    EXPECT_EQ(parent_directory_levels("a/../../b.txt"), 1);
    EXPECT_EQ(parent_directory_levels("a/../../b/c/d/../../../../test.txt"),
              2);
}

TEST(FileUtilsTest, LastNSegments)
{
    EXPECT_EQ(last_n_segments("abc", 0), "");
    EXPECT_EQ(last_n_segments("abc", 1), "abc");
    EXPECT_ANY_THROW(last_n_segments("abc", 2));
    EXPECT_ANY_THROW(last_n_segments("abc", 3));

    EXPECT_EQ(last_n_segments("/abc", 0), "");
    EXPECT_EQ(last_n_segments("/abc", 1), "abc");
    EXPECT_ANY_THROW(last_n_segments("/abc", 2));
    EXPECT_ANY_THROW(last_n_segments("/abc", 3));

    EXPECT_EQ(last_n_segments("/a/bc", 0), "");
    EXPECT_EQ(last_n_segments("/a/bc", 1), "bc");
    EXPECT_EQ(last_n_segments("/a/bc", 2), "a/bc");
    EXPECT_ANY_THROW(last_n_segments("/a/bc", 3));

    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e", 0), "");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e", 1), "e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e", 2), "dd/e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e", 3), "c/dd/e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e", 4), "bb/c/dd/e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e", 5), "a/bb/c/dd/e");
    EXPECT_ANY_THROW(last_n_segments("/a/bb/c/dd/e", 6));

    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e/", 0), "");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e/", 1), "e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e/", 2), "dd/e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e/", 3), "c/dd/e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e/", 4), "bb/c/dd/e");
    EXPECT_EQ(last_n_segments("/a/bb/c/dd/e/", 5), "a/bb/c/dd/e");
    EXPECT_ANY_THROW(last_n_segments("/a/bb/c/dd/e/", 6));
}

TEST(FileUtilsTest, JoinNormalizePath)
{
    const std::string base = "base/";
    const std::string extension = "/extension";
    const std::string proper = "base/extension";
    const int bLast = base.size() - 1;

    // Both base & extension have slashes
    EXPECT_EQ(join_normalize_path(base, extension), proper);
    // neither have slashes
    EXPECT_EQ(join_normalize_path(base.substr(0, bLast), extension.substr(1)),
              proper);
    // only base has slash
    EXPECT_EQ(join_normalize_path(base, extension.substr(1)), proper);
    // only extension has slash
    EXPECT_EQ(join_normalize_path(base.substr(0, bLast), extension), proper);
    // extension empty, should just normalize base
    EXPECT_EQ(join_normalize_path(base, ""), base.substr(0, bLast));
    // base empty, should just normalize extension
    EXPECT_EQ(join_normalize_path("", extension), extension);
    // both empty, should return empty string
    EXPECT_EQ(join_normalize_path("", ""), "");
}

TEST(FileUtilsTest, ExpandPath)
{
    const std::string home = getenv("HOME");
    const std::string newHome = "tmp";
    setenv("HOME", newHome.c_str(), true);

    EXPECT_EQ(expand_path("~/"), newHome);
    EXPECT_EQ(expand_path("~/fake_file"), newHome + "/fake_file");
    EXPECT_EQ(expand_path("fake_dir/fake_file"), "fake_dir/fake_file");

    setenv("HOME", "", true);
    ASSERT_THROW(expand_path("~/"), std::runtime_error);
    EXPECT_EQ(expand_path("fake_dir/fake_file"), "fake_dir/fake_file");
    EXPECT_EQ(expand_path(""), "");
    // make sure $HOME is reset so other tests don't break
    setenv("HOME", home.c_str(), true);
    EXPECT_EQ(getenv("HOME"), home);
}
