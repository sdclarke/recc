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

#include <subprocess.h>

#include <gtest/gtest.h>

#include <exception>
#include <sys/stat.h>

using namespace BloombergLP::recc;
using namespace std;

TEST(FileUtilsTest, TemporaryDirectory)
{
    string name;
    {
        TemporaryDirectory tempDir("test-prefix");
        name = string(tempDir.name());
        EXPECT_NE(name.find("test-prefix"), string::npos);

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
    string name = tempDir.name() + string("/some/directory/path");

    create_directory_recursive(name.c_str());

    struct stat statResult;
    ASSERT_EQ(stat(name.c_str(), &statResult), 0);
    ASSERT_TRUE(S_ISDIR(statResult.st_mode));
}

TEST(FileUtilsTest, Executable)
{
    TemporaryDirectory tempDir;
    string fileName = tempDir.name() + string("/testfile");

    ASSERT_THROW(is_executable(fileName.c_str()), exception);
    ASSERT_THROW(make_executable(fileName.c_str()), exception);

    write_file(fileName.c_str(), ""); // Create a test file.

    EXPECT_FALSE(is_executable(fileName.c_str()));
    make_executable(fileName.c_str());
    EXPECT_TRUE(is_executable(fileName.c_str()));
}

TEST(FileUtilsTest, FileContents)
{
    TemporaryDirectory tempDir;
    string fileName = tempDir.name() + string("/testfile");

    EXPECT_THROW(get_file_contents(fileName.c_str()), exception);

    write_file(fileName.c_str(), "File contents");
    EXPECT_EQ(get_file_contents(fileName.c_str()), "File contents");

    write_file(fileName.c_str(), "Overwrite, don't append");
    EXPECT_EQ(get_file_contents(fileName.c_str()), "Overwrite, don't append");
}

TEST(FileUtilsTest, FileContentsCreatesDirectory)
{
    TemporaryDirectory tempDir;
    string fileName = tempDir.name() + string("/some/subdirectory/file.txt");

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

TEST(MakePathRelativeTest, ReturnNonAbsolutePathsUnmodified)
{
    EXPECT_EQ("", make_path_relative(string(""), "/some/working/directory"));
    EXPECT_EQ("", make_path_relative(string(""), "/some/working/directory"));
    EXPECT_EQ("test",
              make_path_relative(string("test"), "/some/working/directory"));
    EXPECT_EQ("test/long/path", make_path_relative(string("test/long/path"),
                                                   "/some/working/directory"));
    EXPECT_EQ("some/path", make_path_relative(string("some/path"),
                                              "/some/working/directory"));
}

TEST(MakePathRelativeTest, DoNothingIfWorkingDirectoryNull)
{
    EXPECT_EQ("/test/directory/",
              make_path_relative(string("/test/directory/"), nullptr));
    EXPECT_EQ("/test/directory/",
              make_path_relative(string("/test/directory/"), ""));
    EXPECT_EQ("/test", make_path_relative(string("/test"), ""));
}

TEST(MakePathRelativeTest, WorkingDirectoryIsPathPrefix)
{
    EXPECT_EQ("path",
              make_path_relative(string("/some/test/path"), "/some/test"));
    EXPECT_EQ("path",
              make_path_relative(string("/some/test/path"), "/some/test/"));
    EXPECT_EQ("path/",
              make_path_relative(string("/some/test/path/"), "/some/test"));
    EXPECT_EQ("path/",
              make_path_relative(string("/some/test/path/"), "/some/test/"));
}

TEST(MakePathRelativeTest, PathEqualsWorkingDirectory)
{
    EXPECT_EQ(
        ".", make_path_relative(string("/some/test/path"), "/some/test/path"));
    EXPECT_EQ(".", make_path_relative(string("/some/test/path"),
                                      "/some/test/path/"));
    EXPECT_EQ("./", make_path_relative(string("/some/test/path/"),
                                       "/some/test/path"));
    EXPECT_EQ("./", make_path_relative(string("/some/test/path/"),
                                       "/some/test/path/"));
}

TEST(MakePathRelativeTest, PathAlmostEqualsWorkingDirectory)
{
    EXPECT_EQ("../tests",
              make_path_relative(string("/some/tests"), "/some/test"));
    EXPECT_EQ("../tests",
              make_path_relative(string("/some/tests"), "/some/test/"));
    EXPECT_EQ("../tests/",
              make_path_relative(string("/some/tests/"), "/some/test"));
    EXPECT_EQ("../tests/",
              make_path_relative(string("/some/tests/"), "/some/test/"));
}

TEST(MakePathRelativeTest, PathIsParentOfWorkingDirectory)
{
    EXPECT_EQ("..", make_path_relative(string("/a/b/c"), "/a/b/c/d"));
    EXPECT_EQ("..", make_path_relative(string("/a/b/c"), "/a/b/c/d/"));
    EXPECT_EQ("../", make_path_relative(string("/a/b/c/"), "/a/b/c/d"));
    EXPECT_EQ("../", make_path_relative(string("/a/b/c/"), "/a/b/c/d/"));

    EXPECT_EQ("../../..", make_path_relative(string("/a"), "/a/b/c/d"));
    EXPECT_EQ("../../..", make_path_relative(string("/a"), "/a/b/c/d/"));
    EXPECT_EQ("../../../", make_path_relative(string("/a/"), "/a/b/c/d"));
    EXPECT_EQ("../../../", make_path_relative(string("/a/"), "/a/b/c/d/"));
}

TEST(MakePathRelativeTest, PathAdjacentToWorkingDirectory)
{
    EXPECT_EQ("../e", make_path_relative(string("/a/b/c/e"), "/a/b/c/d"));
    EXPECT_EQ("../e", make_path_relative(string("/a/b/c/e"), "/a/b/c/d/"));
    EXPECT_EQ("../e/", make_path_relative(string("/a/b/c/e/"), "/a/b/c/d"));
    EXPECT_EQ("../e/", make_path_relative(string("/a/b/c/e/"), "/a/b/c/d/"));

    EXPECT_EQ("../e/f/g",
              make_path_relative(string("/a/b/c/e/f/g"), "/a/b/c/d"));
    EXPECT_EQ("../e/f/g",
              make_path_relative(string("/a/b/c/e/f/g"), "/a/b/c/d/"));
    EXPECT_EQ("../e/f/g/",
              make_path_relative(string("/a/b/c/e/f/g/"), "/a/b/c/d"));
    EXPECT_EQ("../e/f/g/",
              make_path_relative(string("/a/b/c/e/f/g/"), "/a/b/c/d/"));

    EXPECT_EQ("../../e/f/g",
              make_path_relative(string("/a/b/e/f/g"), "/a/b/c/d"));
    EXPECT_EQ("../../e/f/g",
              make_path_relative(string("/a/b/e/f/g"), "/a/b/c/d/"));
    EXPECT_EQ("../../e/f/g/",
              make_path_relative(string("/a/b/e/f/g/"), "/a/b/c/d"));
    EXPECT_EQ("../../e/f/g/",
              make_path_relative(string("/a/b/e/f/g/"), "/a/b/c/d/"));
}

TEST(MakePathRelativeTest, PathUnrelatedToWorkingDirectory)
{
    EXPECT_EQ("/e/b/c/d", make_path_relative(string("/e/b/c/d"), "/a/b/c/d"));
    EXPECT_EQ("/e/b/c/d", make_path_relative(string("/e/b/c/d"), "/a/b/c/d/"));
    EXPECT_EQ("/e/b/c/d/",
              make_path_relative(string("/e/b/c/d/"), "/a/b/c/d"));
    EXPECT_EQ("/e/b/c/d/",
              make_path_relative(string("/e/b/c/d/"), "/a/b/c/d/"));
}

TEST(FileUtilsTest, GetCurrentWorkingDirectory)
{
    const vector<string> command = {"pwd"};
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
