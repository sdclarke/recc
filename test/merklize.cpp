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
#include <gtest/gtest.h>
#include <merklize.h>

using namespace BloombergLP::recc;

TEST(DigestTest, EmptyDigest)
{
    auto digest = make_digest(std::string(""));
    EXPECT_EQ(0, digest.size_bytes());
    EXPECT_EQ(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        digest.hash());
}

TEST(DigestTest, TrivialDigest)
{
    auto digest = make_digest(std::string("abc"));
    EXPECT_EQ(3, digest.size_bytes());
    EXPECT_EQ(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        digest.hash());
}

TEST(FileTest, TrivialFile)
{
    File file("abc.txt");
    EXPECT_EQ(3, file.d_digest.size_bytes());
    EXPECT_EQ(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        file.d_digest.hash());
    EXPECT_FALSE(file.d_executable);
}

TEST(FileTest, ExecutableFile)
{
    File file("script.sh");
    EXPECT_EQ(41, file.d_digest.size_bytes());
    EXPECT_EQ(
        "a56e86eefe699eb6a759ff6ddf94ca54efc2f6463946b9585858511e07c88b8c",
        file.d_digest.hash());
    EXPECT_TRUE(file.d_executable);
}

TEST(FileTest, ToFilenode)
{
    File file;
    file.d_digest.set_hash("HASH HERE");
    file.d_digest.set_size_bytes(123);
    file.d_executable = true;

    auto fileNode = file.to_filenode(std::string("file.name"));

    EXPECT_EQ(fileNode.name(), "file.name");
    EXPECT_EQ(fileNode.digest().hash(), "HASH HERE");
    EXPECT_EQ(fileNode.digest().size_bytes(), 123);
    EXPECT_TRUE(fileNode.is_executable());
}

TEST(NestedDirectoryTest, EmptyNestedDirectory)
{
    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = NestedDirectory().to_digest(&digestMap);
    EXPECT_EQ(1, digestMap.size());
    ASSERT_EQ(1, digestMap.count(digest));

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.files_size());
    EXPECT_EQ(0, message.directories_size());
}

TEST(NestedDirectoryTest, TrivialNestedDirectory)
{
    File file;
    file.d_digest.set_hash("DIGESTHERE");

    NestedDirectory directory;
    directory.add(file, "sample");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);
    EXPECT_EQ(1, digestMap.size());
    ASSERT_EQ(1, digestMap.count(digest));

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.directories_size());
    ASSERT_EQ(1, message.files_size());
    EXPECT_EQ("sample", message.files(0).name());
    EXPECT_EQ("DIGESTHERE", message.files(0).digest().hash());
}

TEST(NestedDirectoryTest, Subdirectories)
{
    File file;
    file.d_digest.set_hash("HASH1");

    File file2;
    file2.d_digest.set_hash("HASH2");

    NestedDirectory directory;
    directory.add(file, "sample");
    directory.add(file2, "subdir/anothersubdir/sample2");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);
    EXPECT_EQ(3, digestMap.size());
    ASSERT_EQ(1, digestMap.count(digest));

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);

    EXPECT_EQ(1, message.files_size());
    EXPECT_EQ("sample", message.files(0).name());
    EXPECT_EQ("HASH1", message.files(0).digest().hash());
    ASSERT_EQ(1, message.directories_size());
    EXPECT_EQ("subdir", message.directories(0).name());

    ASSERT_EQ(1, digestMap.count(message.directories(0).digest()));
    proto::Directory subdir1;
    subdir1.ParseFromString(digestMap[message.directories(0).digest()]);
    EXPECT_EQ(0, subdir1.files_size());
    ASSERT_EQ(1, subdir1.directories_size());
    EXPECT_EQ("anothersubdir", subdir1.directories(0).name());

    ASSERT_EQ(1, digestMap.count(subdir1.directories(0).digest()));
    proto::Directory subdir2;
    subdir2.ParseFromString(digestMap[subdir1.directories(0).digest()]);
    EXPECT_EQ(0, subdir2.directories_size());
    ASSERT_EQ(1, subdir2.files_size());
    EXPECT_EQ("sample2", subdir2.files(0).name());
    EXPECT_EQ("HASH2", subdir2.files(0).digest().hash());
}

TEST(NestedDirectoryTest, AddSingleDirectory)
{
    NestedDirectory directory;
    directory.addDirectory("foo");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.files_size());
    ASSERT_EQ(1, message.directories_size());
    EXPECT_EQ("foo", message.directories(0).name());
}

TEST(NestedDirectoryTest, AddSlashDirectory)
{
    NestedDirectory directory;
    directory.addDirectory("/");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.files_size());
    ASSERT_EQ(0, message.directories_size());
}

TEST(NestedDirectoryTest, AddAbsoluteDirectory)
{
    NestedDirectory directory;
    directory.addDirectory("/root");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.files_size());
    ASSERT_EQ(1, message.directories_size());
    EXPECT_EQ("root", message.directories(0).name());
}

TEST(NestedDirectoryTest, EmptySubdirectories)
{

    NestedDirectory directory;
    directory.addDirectory("foo/bar/baz");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.files_size());
    ASSERT_EQ(1, message.directories_size());
    EXPECT_EQ("foo", message.directories(0).name());

    proto::Directory subdir;
    subdir.ParseFromString(digestMap[message.directories(0).digest()]);
    EXPECT_EQ(0, message.files_size());
    EXPECT_EQ(1, subdir.directories_size());
    EXPECT_EQ("bar", subdir.directories(0).name());

    proto::Directory subdir2;
    subdir.ParseFromString(digestMap[subdir.directories(0).digest()]);
    EXPECT_EQ(0, message.files_size());
    EXPECT_EQ(1, subdir.directories_size());
    EXPECT_EQ("baz", subdir.directories(0).name());
}

TEST(NestedDirectoryTest, AddDirsToExistingNestedDirectory)
{
    File file;
    file.d_digest.set_hash("DIGESTHERE");

    NestedDirectory directory;
    directory.add(file, "directory/file");
    directory.addDirectory("directory/foo");
    directory.addDirectory("otherdir");

    std::unordered_map<proto::Digest, std::string> digestMap;
    auto digest = directory.to_digest(&digestMap);

    proto::Directory message;
    message.ParseFromString(digestMap[digest]);
    EXPECT_EQ(0, message.files_size());
    ASSERT_EQ(2, message.directories_size());
    EXPECT_EQ("directory", message.directories(0).name());
    EXPECT_EQ("otherdir", message.directories(1).name());

    proto::Directory subdir;
    subdir.ParseFromString(digestMap[message.directories(0).digest()]);
    EXPECT_EQ(1, subdir.files_size());
    EXPECT_EQ(1, subdir.directories_size());
    EXPECT_EQ("file", subdir.files(0).name());
    EXPECT_EQ("foo", subdir.directories(0).name());
}

TEST(NestedDirectoryTest, MakeNestedDirectory)
{
    std::unordered_map<proto::Digest, std::string> fileMap;
    auto nestedDirectory = make_nesteddirectory(".", &fileMap);

    EXPECT_EQ(1, nestedDirectory.d_subdirs->size());
    EXPECT_EQ(2, nestedDirectory.d_files.size());

    EXPECT_EQ(
        "abc",
        get_file_contents(
            fileMap[nestedDirectory.d_files["abc.txt"].d_digest].c_str()));

    auto subdirectory = &(*nestedDirectory.d_subdirs)["subdir"];
    EXPECT_EQ(0, subdirectory->d_subdirs->size());
    EXPECT_EQ(1, subdirectory->d_files.size());
    EXPECT_EQ("abc",
              get_file_contents(
                  fileMap[subdirectory->d_files["abc.txt"].d_digest].c_str()));
}

// Make sure the digest is calculated correctly regardless of the order in
// which the files are added. Important for caching.
TEST(NestedDirectoryTest, ConsistentDigestRegardlessOfFileOrder)
{
    int N = 5;
    // Get us some mock files
    File files[N];
    for (int i = 0; i < N; i++) {
        files[i].d_digest.set_hash("HASH_" + std::to_string(i));
    }

    // Create Nested Directory and add everything in-order
    NestedDirectory directory1;
    for (int i = 0; i < N; i++) {
        std::string fn =
            "subdir_" + std::to_string(i) + "/file_" + std::to_string(i);
        directory1.add(files[i], fn.c_str());
    }

    // Create another Nested Directory and add everything in reverse order
    NestedDirectory directory2;
    for (int i = N - 1; i >= 0; i--) {
        std::string fn =
            "subdir_" + std::to_string(i) + "/file_" + std::to_string(i);
        directory2.add(files[i], fn.c_str());
    }

    // Make sure the actual digests of those two directories are identical
    EXPECT_EQ(directory1.to_digest().hash(), directory2.to_digest().hash());
}

// Make sure digests of directories containing different files are different
TEST(NestedDirectoryTest, NestedDirectoryDigestsReallyBasedOnFiles)
{
    int N = 5;
    // Get us some mock files
    File files_dir1[N]; // Files to add in the first directory
    File files_dir2[N]; // Files to add in the second directory
    for (int i = 0; i < N; i++) {
        files_dir1[i].d_digest.set_hash("HASH_DIR1_" + std::to_string(i));
        files_dir2[i].d_digest.set_hash("HASH_DIR2_" + std::to_string(i));
    }

    // Create Nested Directories and add everything in-order
    NestedDirectory directory1, directory2;
    for (int i = 0; i < N; i++) {
        std::string fn =
            "subdir_" + std::to_string(i) + "/file_" + std::to_string(i);
        directory1.add(files_dir1[i], fn.c_str());
        directory2.add(files_dir2[i], fn.c_str());
    }

    // Make sure the digests are different
    EXPECT_NE(directory1.to_digest().hash(), directory2.to_digest().hash());
}
