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

#include <merklize.h>

#include <fileutils.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace std;

TEST(DigestTest, EmptyDigest)
{
    auto digest = make_digest(string(""));
    EXPECT_EQ(0, digest.size_bytes());
    EXPECT_EQ(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        digest.hash());
}

TEST(DigestTest, TrivialDigest)
{
    auto digest = make_digest(string("abc"));
    EXPECT_EQ(3, digest.size_bytes());
    EXPECT_EQ(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        digest.hash());
}

TEST(FileTest, TrivialFile)
{
    File file("abc.txt");
    EXPECT_EQ(3, file.digest.size_bytes());
    EXPECT_EQ(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        file.digest.hash());
    EXPECT_FALSE(file.executable);
}

TEST(FileTest, ExecutableFile)
{
    File file("script.sh");
    EXPECT_EQ(41, file.digest.size_bytes());
    EXPECT_EQ(
        "a56e86eefe699eb6a759ff6ddf94ca54efc2f6463946b9585858511e07c88b8c",
        file.digest.hash());
    EXPECT_TRUE(file.executable);
}

TEST(FileTest, ToFilenode)
{
    File file;
    file.digest.set_hash("HASH HERE");
    file.digest.set_size_bytes(123);
    file.executable = true;

    auto fileNode = file.to_filenode(string("file.name"));

    EXPECT_EQ(fileNode.name(), "file.name");
    EXPECT_EQ(fileNode.digest().hash(), "HASH HERE");
    EXPECT_EQ(fileNode.digest().size_bytes(), 123);
    EXPECT_TRUE(fileNode.is_executable());
}

TEST(NestedDirectoryTest, EmptyNestedDirectory)
{
    unordered_map<proto::Digest, string> digestMap;
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
    file.digest.set_hash("DIGESTHERE");

    NestedDirectory directory;
    directory.add(file, "sample");

    unordered_map<proto::Digest, string> digestMap;
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
    file.digest.set_hash("HASH1");

    File file2;
    file2.digest.set_hash("HASH2");

    NestedDirectory directory;
    directory.add(file, "sample");
    directory.add(file2, "subdir/anothersubdir/sample2");

    unordered_map<proto::Digest, string> digestMap;
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

TEST(NestedDirectoryTest, SubdirectoriesToTree)
{
    File file;
    file.digest.set_hash("HASH1");

    File file2;
    file2.digest.set_hash("HASH2");

    NestedDirectory directory;
    directory.add(file, "sample");
    directory.add(file2, "subdir/anothersubdir/sample2");

    auto tree = directory.to_tree();
    EXPECT_EQ(2, tree.children_size());

    unordered_map<proto::Digest, proto::Directory> digestMap;
    for (auto &child : tree.children()) {
        digestMap[make_digest(child)] = child;
    }

    auto root = tree.root();

    EXPECT_EQ(1, root.files_size());
    EXPECT_EQ("sample", root.files(0).name());
    EXPECT_EQ("HASH1", root.files(0).digest().hash());
    ASSERT_EQ(1, root.directories_size());
    EXPECT_EQ("subdir", root.directories(0).name());

    ASSERT_EQ(1, digestMap.count(root.directories(0).digest()));
    proto::Directory subdir1 = digestMap[root.directories(0).digest()];
    EXPECT_EQ(0, subdir1.files_size());
    ASSERT_EQ(1, subdir1.directories_size());
    EXPECT_EQ("anothersubdir", subdir1.directories(0).name());

    ASSERT_EQ(1, digestMap.count(subdir1.directories(0).digest()));
    proto::Directory subdir2 = digestMap[subdir1.directories(0).digest()];
    EXPECT_EQ(0, subdir2.directories_size());
    ASSERT_EQ(1, subdir2.files_size());
    EXPECT_EQ("sample2", subdir2.files(0).name());
    EXPECT_EQ("HASH2", subdir2.files(0).digest().hash());
}

TEST(NestedDirectoryTest, MakeNestedDirectory)
{
    unordered_map<proto::Digest, string> fileMap;
    auto nestedDirectory = make_nesteddirectory(".", &fileMap);

    EXPECT_EQ(1, nestedDirectory.subdirs->size());
    EXPECT_EQ(2, nestedDirectory.files.size());

    EXPECT_EQ("abc",
              get_file_contents(
                  fileMap[nestedDirectory.files["abc.txt"].digest].c_str()));

    auto subdirectory = &(*nestedDirectory.subdirs)["subdir"];
    EXPECT_EQ(0, subdirectory->subdirs->size());
    EXPECT_EQ(1, subdirectory->files.size());
    EXPECT_EQ("abc",
              get_file_contents(
                  fileMap[subdirectory->files["abc.txt"].digest].c_str()));
}
