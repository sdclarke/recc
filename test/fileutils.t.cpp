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
    const std::string name =
        tempDir.name() + std::string("/some/directory/path");

    FileUtils::createDirectoryRecursive(name);

    struct stat statResult;
    ASSERT_EQ(stat(name.c_str(), &statResult), 0);
    ASSERT_TRUE(S_ISDIR(statResult.st_mode));
}

TEST(FileUtilsTest, Executable)
{
    TemporaryDirectory tempDir;
    const std::string fileName = tempDir.name() + std::string("/testfile.txt");

    ASSERT_THROW(FileUtils::isExecutable(fileName), std::exception);
    ASSERT_THROW(FileUtils::makeExecutable(fileName), std::exception);

    FileUtils::writeFile(fileName, ""); // Create a test file.

    EXPECT_FALSE(FileUtils::isExecutable(fileName));
    FileUtils::makeExecutable(fileName);
    EXPECT_TRUE(FileUtils::isExecutable(fileName));
}

TEST(FileUtilsTest, FileContents)
{
    TemporaryDirectory tempDir;
    const std::string fileName = tempDir.name() + std::string("/testfile.txt");

    EXPECT_THROW(FileUtils::getFileContents(fileName), std::exception);

    FileUtils::writeFile(fileName, "File contents");
    EXPECT_EQ(FileUtils::getFileContents(fileName), "File contents");

    FileUtils::writeFile(fileName, "Overwrite, don't append");
    EXPECT_EQ(FileUtils::getFileContents(fileName), "Overwrite, don't append");
}

TEST(FileUtilsTest, FileContentsCreatesDirectory)
{
    TemporaryDirectory tempDir;
    const std::string fileName =
        tempDir.name() + std::string("/some/subdirectory/file.txt");

    FileUtils::writeFile(fileName, "File contents");
    EXPECT_EQ(FileUtils::getFileContents(fileName), "File contents");
}

TEST(NormalizePathTest, AlreadyNormalPaths)
{
    EXPECT_EQ("test.txt", FileUtils::normalizePath("test.txt"));
    EXPECT_EQ("subdir/hello", FileUtils::normalizePath("subdir/hello"));
    EXPECT_EQ("/usr/bin/gcc", FileUtils::normalizePath("/usr/bin/gcc"));
}

TEST(NormalizePathTest, RemoveEmptySegments)
{
    EXPECT_EQ("subdir/hello", FileUtils::normalizePath("subdir///hello//"));
    EXPECT_EQ("/usr/bin/gcc", FileUtils::normalizePath("/usr/bin/./gcc"));
}

TEST(NormalizePathTest, RemoveUnneededDotDot)
{
    EXPECT_EQ("subdir/hello",
              FileUtils::normalizePath("subdir/subsubdir/../hello"));
    EXPECT_EQ("/usr/bin/gcc",
              FileUtils::normalizePath("/usr/local/lib/../../bin/.//gcc"));
}

TEST(NormalizePathTest, KeepNeededDotDot)
{
    EXPECT_EQ("../dir/hello", FileUtils::normalizePath("../dir/hello"));
    EXPECT_EQ("../dir/hello",
              FileUtils::normalizePath("subdir/../../dir/hello"));
    EXPECT_EQ("../../dir/hello",
              FileUtils::normalizePath("subdir/../../../dir/hello"));
}

TEST(NormalizePathTest, AlwaysRemoveTrailingSlash)
{
    EXPECT_EQ("/usr/bin", FileUtils::normalizePath("/usr/bin"));
    EXPECT_EQ("/usr/bin", FileUtils::normalizePath("/usr/bin/"));
}

TEST(HasPathPrefixTest, AbsolutePaths)
{
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/b/c/", "/a/b"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/b/c/", "/a/b/"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/b/c", "/a/b"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/b/c", "/a/b/"));

    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/c/d", "/a/b/"));

    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/boo", "/a/b/"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/boo", "/a/b"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/boo", "/a/b/a/boo"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/boo", "/a/b/a/boo/"));

    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/../b/", "/a"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/../b/", "/a/"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/../b", "/a"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/../b", "/a/"));
}

TEST(HasPathPrefixTest, RelativePaths)
{
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/b/c/", "a/b"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/b/c/", "a/b/"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/b/c", "a/b"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/b/c", "a/b/"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("/a/b/c", "/a/b/c"));

    EXPECT_FALSE(FileUtils::hasPathPrefix("a/c/d", "a/b/"));

    EXPECT_FALSE(FileUtils::hasPathPrefix("a/boo", "a/b/"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("a/boo", "a/b"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("a/boo", "a/b/a/boo"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("a/boo", "a/b/a/boo/"));

    EXPECT_TRUE(FileUtils::hasPathPrefix("a/../b/", "a"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/../b/", "a/"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/../b", "a"));
    EXPECT_TRUE(FileUtils::hasPathPrefix("a/../b", "a/"));

    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/b/c/", "a/b/"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/b/c/", "a/b"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/b/c", "a/b/"));
    EXPECT_FALSE(FileUtils::hasPathPrefix("/a/b/c", "a/b"));
}

TEST(HasPathPrefixesTest, PathTests)
{
    const std::set<std::string> prefixes = {"/usr/include",
                                            "/opt/rh/devtoolset-7"};

    EXPECT_TRUE(FileUtils::hasPathPrefixes("/usr/include/stat.h", prefixes));
    EXPECT_FALSE(FileUtils::hasPathPrefixes("usr/include/stat.h", prefixes));
    EXPECT_TRUE(
        FileUtils::hasPathPrefixes("/opt/rh/devtoolset-7/foo.h", prefixes));
    EXPECT_FALSE(FileUtils::hasPathPrefixes("/opt/rh/foo.h", prefixes));

    EXPECT_TRUE(FileUtils::hasPathPrefixes("/some/dir/foo.h", {"/"}));
    EXPECT_FALSE(FileUtils::hasPathPrefixes("/", {"/some/other/dir"}));

    EXPECT_TRUE(FileUtils::hasPathPrefixes("/some/dir,withcomma/foo.h",
                                           {"/some/dir,withcomma/"}));
}

class MakePathRelativeTest : public ::testing::Test {
  protected:
    void SetUp() override { RECC_PROJECT_ROOT = "/"; }
};

TEST_F(MakePathRelativeTest, ReturnNonAbsolutePathsUnmodified)
{
    EXPECT_EQ("", FileUtils::makePathRelative(std::string(""),
                                              "/some/working/directory"));
    EXPECT_EQ("", FileUtils::makePathRelative(std::string(""),
                                              "/some/working/directory"));
    EXPECT_EQ("test", FileUtils::makePathRelative(std::string("test"),
                                                  "/some/working/directory"));
    EXPECT_EQ("test/long/path",
              FileUtils::makePathRelative(std::string("test/long/path"),
                                          "/some/working/directory"));
    EXPECT_EQ("some/path",
              FileUtils::makePathRelative(std::string("some/path"),
                                          "/some/working/directory"));
}

TEST_F(MakePathRelativeTest, DoNothingIfWorkingDirectoryNull)
{
    EXPECT_EQ("/test/directory/",
              FileUtils::makePathRelative("/test/directory/", ""));
    EXPECT_EQ("/test", FileUtils::makePathRelative("/test", ""));
}

TEST_F(MakePathRelativeTest, WorkingDirectoryIsPathPrefix)
{
    EXPECT_EQ("path", FileUtils::makePathRelative(
                          std::string("/some/test/path"), "/some/test"));
    EXPECT_EQ("path", FileUtils::makePathRelative(
                          std::string("/some/test/path"), "/some/test/"));
    EXPECT_EQ("path/", FileUtils::makePathRelative(
                           std::string("/some/test/path/"), "/some/test"));
    EXPECT_EQ("path/", FileUtils::makePathRelative(
                           std::string("/some/test/path/"), "/some/test/"));
}

TEST_F(MakePathRelativeTest, PathEqualsWorkingDirectory)
{
    EXPECT_EQ(".", FileUtils::makePathRelative(std::string("/some/test/path"),
                                               "/some/test/path"));
    EXPECT_EQ(".", FileUtils::makePathRelative(std::string("/some/test/path"),
                                               "/some/test/path/"));
    EXPECT_EQ("./", FileUtils::makePathRelative(
                        std::string("/some/test/path/"), "/some/test/path"));
    EXPECT_EQ("./", FileUtils::makePathRelative(
                        std::string("/some/test/path/"), "/some/test/path/"));
}

TEST_F(MakePathRelativeTest, PathAlmostEqualsWorkingDirectory)
{
    EXPECT_EQ("../tests", FileUtils::makePathRelative(
                              std::string("/some/tests"), "/some/test"));
    EXPECT_EQ("../tests", FileUtils::makePathRelative(
                              std::string("/some/tests"), "/some/test/"));
    EXPECT_EQ("../tests/", FileUtils::makePathRelative(
                               std::string("/some/tests/"), "/some/test"));
    EXPECT_EQ("../tests/", FileUtils::makePathRelative(
                               std::string("/some/tests/"), "/some/test/"));
}

TEST_F(MakePathRelativeTest, PathIsParentOfWorkingDirectory)
{
    EXPECT_EQ("..",
              FileUtils::makePathRelative(std::string("/a/b/c"), "/a/b/c/d"));
    EXPECT_EQ("..",
              FileUtils::makePathRelative(std::string("/a/b/c"), "/a/b/c/d/"));
    EXPECT_EQ("../",
              FileUtils::makePathRelative(std::string("/a/b/c/"), "/a/b/c/d"));
    EXPECT_EQ("../", FileUtils::makePathRelative(std::string("/a/b/c/"),
                                                 "/a/b/c/d/"));

    EXPECT_EQ("../../..",
              FileUtils::makePathRelative(std::string("/a"), "/a/b/c/d"));
    EXPECT_EQ("../../..",
              FileUtils::makePathRelative(std::string("/a"), "/a/b/c/d/"));
    EXPECT_EQ("../../../",
              FileUtils::makePathRelative(std::string("/a/"), "/a/b/c/d"));
    EXPECT_EQ("../../../",
              FileUtils::makePathRelative(std::string("/a/"), "/a/b/c/d/"));
}

TEST_F(MakePathRelativeTest, PathAdjacentToWorkingDirectory)
{
    EXPECT_EQ("../e", FileUtils::makePathRelative(std::string("/a/b/c/e"),
                                                  "/a/b/c/d"));
    EXPECT_EQ("../e", FileUtils::makePathRelative(std::string("/a/b/c/e"),
                                                  "/a/b/c/d/"));
    EXPECT_EQ("../e/", FileUtils::makePathRelative(std::string("/a/b/c/e/"),
                                                   "/a/b/c/d"));
    EXPECT_EQ("../e/", FileUtils::makePathRelative(std::string("/a/b/c/e/"),
                                                   "/a/b/c/d/"));

    EXPECT_EQ("../e/f/g", FileUtils::makePathRelative(
                              std::string("/a/b/c/e/f/g"), "/a/b/c/d"));
    EXPECT_EQ("../e/f/g", FileUtils::makePathRelative(
                              std::string("/a/b/c/e/f/g"), "/a/b/c/d/"));
    EXPECT_EQ("../e/f/g/", FileUtils::makePathRelative(
                               std::string("/a/b/c/e/f/g/"), "/a/b/c/d"));
    EXPECT_EQ("../e/f/g/", FileUtils::makePathRelative(
                               std::string("/a/b/c/e/f/g/"), "/a/b/c/d/"));

    EXPECT_EQ("../../e/f/g", FileUtils::makePathRelative(
                                 std::string("/a/b/e/f/g"), "/a/b/c/d"));
    EXPECT_EQ("../../e/f/g", FileUtils::makePathRelative(
                                 std::string("/a/b/e/f/g"), "/a/b/c/d/"));
    EXPECT_EQ("../../e/f/g/", FileUtils::makePathRelative(
                                  std::string("/a/b/e/f/g/"), "/a/b/c/d"));
    EXPECT_EQ("../../e/f/g/", FileUtils::makePathRelative(
                                  std::string("/a/b/e/f/g/"), "/a/b/c/d/"));
}

TEST_F(MakePathRelativeTest, PathOutsideProjectRoot)
{
    RECC_PROJECT_ROOT = "/home/nobody/test/";
    EXPECT_EQ(
        "/home/nobody/include/foo.h",
        FileUtils::makePathRelative(std::string("/home/nobody/include/foo.h"),
                                    "/home/nobody/test"));
    EXPECT_EQ(
        "/home/nobody/include/foo.h",
        FileUtils::makePathRelative(std::string("/home/nobody/include/foo.h"),
                                    "/home/nobody/test"));
}

TEST(MakePathAbsoluteTest, NoRewriting)
{
    EXPECT_EQ("", FileUtils::makePathAbsolute("", "/a/b/c/"));
    EXPECT_EQ("/a/b/c/", FileUtils::makePathAbsolute("/a/b/c/", "/d/"));
}

TEST(MakePathAbsoluteTest, SimplePaths)
{
    EXPECT_EQ("/a/b/c/d", FileUtils::makePathAbsolute("d", "/a/b/c/"));
    EXPECT_EQ("/a/b/c/d/", FileUtils::makePathAbsolute("d/", "/a/b/c/"));
    EXPECT_EQ("/a/b", FileUtils::makePathAbsolute("..", "/a/b/c/"));
    EXPECT_EQ("/a/b/", FileUtils::makePathAbsolute("../", "/a/b/c/"));
    EXPECT_EQ("/a/b", FileUtils::makePathAbsolute("..", "/a/b/c"));
    EXPECT_EQ("/a/b/", FileUtils::makePathAbsolute("../", "/a/b/c"));

    EXPECT_EQ("/a/b/c", FileUtils::makePathAbsolute(".", "/a/b/c/"));
    EXPECT_EQ("/a/b/c/", FileUtils::makePathAbsolute("./", "/a/b/c/"));
    EXPECT_EQ("/a/b/c", FileUtils::makePathAbsolute(".", "/a/b/c"));
    EXPECT_EQ("/a/b/c/", FileUtils::makePathAbsolute("./", "/a/b/c"));
}

TEST(MakePathAbsoluteTest, MoreComplexPaths)
{
    EXPECT_EQ("/a/b/d", FileUtils::makePathAbsolute("../d", "/a/b/c"));
    EXPECT_EQ("/a/b/d", FileUtils::makePathAbsolute("../d", "/a/b/c/"));
    EXPECT_EQ("/a/b/d/", FileUtils::makePathAbsolute("../d/", "/a/b/c"));
    EXPECT_EQ("/a/b/d/", FileUtils::makePathAbsolute("../d/", "/a/b/c/"));

    EXPECT_EQ("/a/b/d", FileUtils::makePathAbsolute("./.././d", "/a/b/c"));
    EXPECT_EQ("/a/b/d", FileUtils::makePathAbsolute("./.././d", "/a/b/c/"));
    EXPECT_EQ("/a/b/d/", FileUtils::makePathAbsolute("./.././d/", "/a/b/c"));
    EXPECT_EQ("/a/b/d/", FileUtils::makePathAbsolute("./.././d/", "/a/b/c/"));
}

TEST(FileUtilsTest, GetCurrentWorkingDirectory)
{
    const std::vector<std::string> command = {"pwd"};
    const auto commandResult = Subprocess::execute(command, true);
    if (commandResult.d_exitCode == 0) {
        EXPECT_EQ(commandResult.d_stdOut,
                  FileUtils::getCurrentWorkingDirectory() + "\n");
    }
}

TEST(FileUtilsTest, ParentDirectoryLevels)
{
    EXPECT_EQ(FileUtils::parentDirectoryLevels(""), 0);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("/"), 0);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("."), 0);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("./"), 0);

    EXPECT_EQ(FileUtils::parentDirectoryLevels(".."), 1);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("../"), 1);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("../.."), 2);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("../../"), 2);

    EXPECT_EQ(FileUtils::parentDirectoryLevels("a/b/c.txt"), 0);
    EXPECT_EQ(FileUtils::parentDirectoryLevels("a/../../b.txt"), 1);
    EXPECT_EQ(
        FileUtils::parentDirectoryLevels("a/../../b/c/d/../../../../test.txt"),
        2);
}

TEST(FileUtilsTest, LastNSegments)
{
    EXPECT_EQ(FileUtils::lastNSegments("abc", 0), "");
    EXPECT_EQ(FileUtils::lastNSegments("abc", 1), "abc");
    EXPECT_ANY_THROW(FileUtils::lastNSegments("abc", 2));
    EXPECT_ANY_THROW(FileUtils::lastNSegments("abc", 3));

    EXPECT_EQ(FileUtils::lastNSegments("/abc", 0), "");
    EXPECT_EQ(FileUtils::lastNSegments("/abc", 1), "abc");
    EXPECT_ANY_THROW(FileUtils::lastNSegments("/abc", 2));
    EXPECT_ANY_THROW(FileUtils::lastNSegments("/abc", 3));

    EXPECT_EQ(FileUtils::lastNSegments("/a/bc", 0), "");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bc", 1), "bc");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bc", 2), "a/bc");
    EXPECT_ANY_THROW(FileUtils::lastNSegments("/a/bc", 3));

    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e", 0), "");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e", 1), "e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e", 2), "dd/e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e", 3), "c/dd/e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e", 4), "bb/c/dd/e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e", 5), "a/bb/c/dd/e");
    EXPECT_ANY_THROW(FileUtils::lastNSegments("/a/bb/c/dd/e", 6));

    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e/", 0), "");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e/", 1), "e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e/", 2), "dd/e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e/", 3), "c/dd/e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e/", 4), "bb/c/dd/e");
    EXPECT_EQ(FileUtils::lastNSegments("/a/bb/c/dd/e/", 5), "a/bb/c/dd/e");
    EXPECT_ANY_THROW(FileUtils::lastNSegments("/a/bb/c/dd/e/", 6));
}

TEST(FileUtilsTest, JoinNormalizePath)
{
    const std::string base = "base/";
    const std::string extension = "/extension";
    const std::string proper = "base/extension";
    const int bLast = base.size() - 1;

    // Both base & extension have slashes
    EXPECT_EQ(FileUtils::joinNormalizePath(base, extension), proper);
    // neither have slashes
    EXPECT_EQ(FileUtils::joinNormalizePath(base.substr(0, bLast),
                                           extension.substr(1)),
              proper);
    // only base has slash
    EXPECT_EQ(FileUtils::joinNormalizePath(base, extension.substr(1)), proper);
    // only extension has slash
    EXPECT_EQ(FileUtils::joinNormalizePath(base.substr(0, bLast), extension),
              proper);
    // extension empty, should just normalize base
    EXPECT_EQ(FileUtils::joinNormalizePath(base, ""), base.substr(0, bLast));
    // base empty, should just normalize extension
    EXPECT_EQ(FileUtils::joinNormalizePath("", extension), extension);
    // both empty, should return empty string
    EXPECT_EQ(FileUtils::joinNormalizePath("", ""), "");
}

TEST(FileUtilsTest, ExpandPath)
{
    const std::string home = getenv("HOME");
    const std::string newHome = "tmp";
    setenv("HOME", newHome.c_str(), true);

    EXPECT_EQ(FileUtils::expandPath("~/"), newHome);
    EXPECT_EQ(FileUtils::expandPath("~/fake_file"), newHome + "/fake_file");
    EXPECT_EQ(FileUtils::expandPath("fake_dir/fake_file"),
              "fake_dir/fake_file");

    setenv("HOME", "", true);
    ASSERT_THROW(FileUtils::expandPath("~/"), std::runtime_error);
    EXPECT_EQ(FileUtils::expandPath("fake_dir/fake_file"),
              "fake_dir/fake_file");
    EXPECT_EQ(FileUtils::expandPath(""), "");
    // make sure $HOME is reset so other tests don't break
    setenv("HOME", home.c_str(), true);
    EXPECT_EQ(getenv("HOME"), home);
}

TEST(FileUtilsTest, AbsolutePaths)
{
    EXPECT_EQ(false, FileUtils::isAbsolutePath("../hello"));
    EXPECT_EQ(true, FileUtils::isAbsolutePath("/../hello/"));
    EXPECT_EQ(false, FileUtils::isAbsolutePath(""));
    EXPECT_EQ(true, FileUtils::isAbsolutePath("/hello/world"));
}

// Test paths are rewritten
TEST(PathRewriteTest, SimpleRewriting)
{
    RECC_PREFIX_REPLACEMENT = {{"/hello/hi", "/hello"},
                               {"/usr/bin/system/bin/hello", "/usr/system"}};
    std::string test_path = "/hello/hi/file.txt";
    ASSERT_EQ("/hello/file.txt",
              FileUtils::resolvePathFromPrefixMap(test_path));

    test_path = "/usr/bin/system/bin/hello/file.txt";
    ASSERT_EQ("/usr/system/file.txt",
              FileUtils::resolvePathFromPrefixMap(test_path));

    test_path = "/hello/bin/not_replaced.txt";
    ASSERT_EQ(test_path, FileUtils::resolvePathFromPrefixMap(test_path));
}

// Test more complicated paths
TEST(PathRewriteTest, ComplicatedPathRewriting)
{
    RECC_PREFIX_REPLACEMENT = {{"/hello/hi", "/hello"},
                               {"/usr/bin/system/bin/hello", "/usr/system"},
                               {"/bin", "/"}};

    auto test_path = "/usr/bin/system/bin/hello/world/";
    ASSERT_EQ("/usr/system/world",
              FileUtils::resolvePathFromPrefixMap(test_path));

    // Don't rewrite non-absolute path
    test_path = "../hello/hi/hi.txt";
    ASSERT_EQ(test_path, FileUtils::resolvePathFromPrefixMap(test_path));

    test_path = "/bin/hello/file.txt";
    ASSERT_EQ("/hello/file.txt",
              FileUtils::resolvePathFromPrefixMap(test_path));
}

TEST(FileUtilsTest, BasenameTest)
{
    EXPECT_EQ("hello", FileUtils::pathBasename("a/b/hello"));
    EXPECT_EQ("hello.txt", FileUtils::pathBasename("a/b/hello.txt"));
    EXPECT_EQ("hello", FileUtils::pathBasename("//hello/a/b/hello"));
    EXPECT_EQ("hello", FileUtils::pathBasename("a/b/../../hello"));
}
