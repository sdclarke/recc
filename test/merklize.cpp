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
#include <fileutils.h>
#include <gtest/gtest.h>
#include <map>
#include <merklize.h>
#include <reccfile.h>
#include <subprocess.h>
#include <vector>

using namespace BloombergLP::recc;

TEST(FileTest, TrivialFile)
{
    const auto path = "abc.txt";
    auto file = ReccFileFactory::createFile(path);
    EXPECT_EQ(3, file->getDigest().size_bytes());
    EXPECT_EQ(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        file->getDigest().hash());
    EXPECT_FALSE(file->isExecutable());
}

TEST(FileTest, ExecutableFile)
{
    const auto path = "script.sh";
    auto file = ReccFileFactory::createFile(path);
    EXPECT_EQ(41, file->getDigest().size_bytes());
    EXPECT_EQ(
        "a56e86eefe699eb6a759ff6ddf94ca54efc2f6463946b9585858511e07c88b8c",
        file->getDigest().hash());
    EXPECT_TRUE(file->isExecutable());
}

TEST(FileTest, ToFilenode)
{
    proto::Digest d;
    d.set_hash("HASH HERE");
    d.set_size_bytes(123);
    ReccFile file("", "", "", d, true);

    auto fileNode = file.getFileNode(std::string("file.name"));

    EXPECT_EQ(fileNode.name(), "file.name");
    EXPECT_EQ(fileNode.digest().hash(), "HASH HERE");
    EXPECT_EQ(fileNode.digest().size_bytes(), 123);
    EXPECT_TRUE(fileNode.is_executable());
}

TEST(NestedDirectoryTest, EmptyNestedDirectory)
{
    digest_string_umap digestMap;
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
    proto::Digest d;
    d.set_hash("DIGESTHERE");
    ReccFile file("", "", "", d, false);

    NestedDirectory directory;
    directory.add(std::make_shared<ReccFile>(file), "sample");

    digest_string_umap digestMap;
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
    proto::Digest d;
    d.set_hash("HASH1");
    ReccFile file("", "", "", d, true);

    proto::Digest d2;
    d2.set_hash("HASH2");
    ReccFile file2("", "", "", d2, true);

    NestedDirectory directory;
    directory.add(std::make_shared<ReccFile>(file), "sample");
    directory.add(std::make_shared<ReccFile>(file2),
                  "subdir/anothersubdir/sample2");

    digest_string_umap digestMap;
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

    digest_string_umap digestMap;
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

    digest_string_umap digestMap;
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

    digest_string_umap digestMap;
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

    digest_string_umap digestMap;
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
    proto::Digest d;
    d.set_hash("DIGESTHERE");
    ReccFile file("", "", "", d, true);

    NestedDirectory directory;
    directory.add(std::make_shared<ReccFile>(file), "directory/file");
    directory.addDirectory("directory/foo");
    directory.addDirectory("otherdir");

    digest_string_umap digestMap;
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
    digest_string_umap fileMap;
    RECC_PROJECT_ROOT = FileUtils::get_current_working_directory();
    std::string cwd = FileUtils::get_current_working_directory();
    auto nestedDirectory = make_nesteddirectory(cwd.c_str(), &fileMap);

    EXPECT_EQ(3, nestedDirectory.d_subdirs->size());
    EXPECT_EQ(2, nestedDirectory.d_files.size());

    EXPECT_EQ("abc", fileMap[nestedDirectory.d_files["abc.txt"]->getDigest()]);

    auto subdirectory = &(*nestedDirectory.d_subdirs)["subdir"];
    EXPECT_EQ(0, subdirectory->d_subdirs->size());
    EXPECT_EQ(1, subdirectory->d_files.size());
    EXPECT_EQ("abc", fileMap[nestedDirectory.d_files["abc.txt"]->getDigest()]);
}

// Make sure the digest is calculated correctly regardless of the order in
// which the files are added. Important for caching.
TEST(NestedDirectoryTest, ConsistentDigestRegardlessOfFileOrder)
{
    int N = 5;
    // Get us some mock digests
    proto::Digest digests[N];
    for (int i = 0; i < N; i++) {
        digests[i].set_hash("HASH_" + std::to_string(i));
    }

    // Make files
    std::vector<std::shared_ptr<ReccFile>> files;
    for (int i = 0; i < N; i++) {
        files.push_back(std::make_shared<ReccFile>(
            ReccFile("", "", "", digests[i], false)));
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
    // Get us some mock digests
    proto::Digest digests1[N];
    proto::Digest digests2[N];

    for (int i = 0; i < N; i++) {
        digests1[i].set_hash("HASH_DIR1_" + std::to_string(i));
        digests2[i].set_hash("HASH_DIR2_" + std::to_string(i));
    }

    // Get us some mock files
    std::vector<std::shared_ptr<ReccFile>> files_dir1;
    std::vector<std::shared_ptr<ReccFile>> files_dir2;

    for (int i = 0; i < N; i++) {
        files_dir1.push_back(std::make_shared<ReccFile>(
            ReccFile("", "", "", digests1[i], false)));
        files_dir2.push_back(std::make_shared<ReccFile>(
            ReccFile("", "", "", digests2[i], false)));
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

TEST(NestedDirectoryTest, NestedDirectoryPathReplacement)
{
    // Replace subdir located in $cwd/data/merklize with $cwd/hi
    // Get current working directory
    std::string cwd = FileUtils::get_current_working_directory();
    RECC_PREFIX_REPLACEMENT = {{cwd + "/nestdir/nestdir2", cwd + "/hi"}};
    digest_string_umap fileMap;
    auto make_nested_dir = cwd + "/nestdir";
    auto nestedDirectory =
        make_nesteddirectory(make_nested_dir.c_str(), &fileMap);

    EXPECT_EQ(1, nestedDirectory.d_subdirs->size());
    EXPECT_EQ(0, nestedDirectory.d_files.size());

    // Make sure that replaced subdirectory exists
    auto subdirectory = &(*nestedDirectory.d_subdirs)["hi"];
    // Make sure [nestdir2] doesn't exist
    EXPECT_EQ((*nestedDirectory.d_subdirs).find("nestdir2"),
              (*nestedDirectory.d_subdirs).end());
    EXPECT_EQ(1, subdirectory->d_subdirs->size());
    EXPECT_EQ(1, subdirectory->d_files.size());
}

TEST(NestedDirectoryTest, NestedDirectoryMultipleNestedDirPathReplacement)
{
    std::string cwd = FileUtils::get_current_working_directory();
    RECC_PREFIX_REPLACEMENT = {
        {cwd + "/nestdir/nestdir2/nestdir3", cwd + "/nestdir"}};
    digest_string_umap fileMap;

    auto dir_to_use = cwd + "/nestdir";
    auto nestedDirectory = make_nesteddirectory(dir_to_use.c_str(), &fileMap);

    auto secondsubDir = &(*nestedDirectory.d_subdirs)["nestdir"];
    auto thirdsubDir = &(*nestedDirectory.d_subdirs)["nestdir2"];
    // Make sure nestdir3 doesn't exist
    EXPECT_EQ(thirdsubDir->d_subdirs->find("nestdir3"),
              thirdsubDir->d_subdirs->end());

    EXPECT_EQ(1, secondsubDir->d_subdirs->size());
    EXPECT_EQ(1, secondsubDir->d_files.size());
    EXPECT_EQ(0, thirdsubDir->d_subdirs->size());
    EXPECT_EQ(0, thirdsubDir->d_files.size());
}

TEST(NestedDirectoryTest, NestedDirectoryNotAPrefix)
{
    RECC_PROJECT_ROOT = FileUtils::get_current_working_directory();
    std::string cwd = FileUtils::get_current_working_directory();
    // not a prefix
    RECC_PREFIX_REPLACEMENT = {{cwd + "/nestdir/nestdir2", "/nestdir/hi"}};
    digest_string_umap fileMap;
    auto nestedDirectory = make_nesteddirectory(cwd.c_str(), &fileMap);

    EXPECT_EQ(3, nestedDirectory.d_subdirs->size());
    EXPECT_EQ(2, nestedDirectory.d_files.size());

    auto secondsubDir = &(*nestedDirectory.d_subdirs)["nestdir"];
    EXPECT_EQ(1, secondsubDir->d_subdirs->size());
    EXPECT_EQ(0, secondsubDir->d_files.size());

    auto subdirectory = &(*secondsubDir->d_subdirs)["nestdir2"];
    EXPECT_EQ(0, subdirectory->d_subdirs->size());
    EXPECT_EQ(0, subdirectory->d_files.size());
}

// Tests nestedDirectory construction with prefix replacement using add.
TEST(NestedDirectoryTest, NestedDirectoryAddPrefixReplacement)
{
    std::string cwd = FileUtils::get_current_working_directory();
    RECC_PREFIX_REPLACEMENT = {{"/nestdir/nestdir2/nestdir3", "/nestdir"}};
    digest_string_umap fileMap;

    NestedDirectory nestdir;

    std::string nestdirfile2 = "/nestdir/nestdir2/nestdir3/nestdirfile2.txt";
    std::string nestdirfiledir = cwd + nestdirfile2;
    auto file = ReccFileFactory::createFile(nestdirfiledir.c_str());
    // add the directory and file, make sure file contents are the same.
    nestdir.add(file, nestdirfile2.c_str());

    EXPECT_EQ(1, nestdir.d_subdirs->size());
    auto subDir = &(*nestdir.d_subdirs)["nestdir"];
    EXPECT_EQ(0, subDir->d_subdirs->size());
    EXPECT_EQ(1, subDir->d_files.size());
    ASSERT_EQ(file->getFileContents(),
              subDir->d_files["nestdirfile2.txt"]->getFileContents());
}

// Tests prefix replacement precidence.
TEST(NestedDirectoryTest, TestPrefixOrder)
{
    // Test that paths are replaced in order specified.
    std::string cwd = FileUtils::get_current_working_directory();
    // Set two of the same prefixes with different replacements, make sure that
    // the first one gets used.
    const auto recc_prefix_string =
        "/nestdir/nestdir2/nestdir3=/nestdir:/nestdir/nestdir2/nestdir3=/nope";

    RECC_PREFIX_REPLACEMENT = vector_from_delimited_string(recc_prefix_string);
    std::unordered_map<proto::Digest, std::string> fileMap;

    NestedDirectory nestdir;

    std::string nestdirfile2 = "/nestdir/nestdir2/nestdir3/nestdirfile2.txt";
    std::string nestdirfiledir = cwd + nestdirfile2;
    auto file = ReccFileFactory::createFile(nestdirfiledir.c_str());
    // add the directory and file, make sure file contents are the same.
    nestdir.add(file, nestdirfile2.c_str());

    EXPECT_EQ(1, nestdir.d_subdirs->size());
    auto subDir = &(*nestdir.d_subdirs)["nestdir"];
    EXPECT_EQ(0, subDir->d_subdirs->size());
    EXPECT_EQ(1, subDir->d_files.size());
    ASSERT_EQ(file->getFileContents(),
              subDir->d_files["nestdirfile2.txt"]->getFileContents());
}

// Tests nestedDirectory construction checking that the entire root is
// replaced.
TEST(NestedDirectoryTest, TestRootReplacement)
{
    // Test that paths are replaced in order specified.
    std::string cwd = FileUtils::get_current_working_directory();

    // Replace the root with "hi"
    const auto recc_prefix_string = "/=/hi";
    RECC_PREFIX_REPLACEMENT = vector_from_delimited_string(recc_prefix_string);
    const std::vector<std::pair<std::string, std::string>> test_vector = {
        {"/", "/hi"}};
    ASSERT_EQ(RECC_PREFIX_REPLACEMENT, test_vector);

    std::unordered_map<proto::Digest, std::string> fileMap;
    NestedDirectory nestdir;
    std::string nestdirfile2 = "/nestdir/nestdir2/nestdir3/nestdirfile2.txt";
    std::string nestdirfiledir = cwd + nestdirfile2;
    auto file = ReccFileFactory::createFile(nestdirfiledir.c_str());
    // add the directory and file, make sure file contents are the same.
    nestdir.add(file, nestdirfile2.c_str());

    EXPECT_EQ(1, nestdir.d_subdirs->size());
    auto subDir = &(*nestdir.d_subdirs)["hi"];
    EXPECT_EQ(1, subDir->d_subdirs->size());
    auto subDir2 = &(*subDir->d_subdirs)["nestdir"];
    EXPECT_EQ(1, subDir2->d_subdirs->size());
    auto subDir3 = &(*subDir2->d_subdirs)["nestdir2"];
    EXPECT_EQ(1, subDir3->d_subdirs->size());
    auto subDir4 = &(*subDir3->d_subdirs)["nestdir3"];
    EXPECT_EQ(0, subDir4->d_subdirs->size());
    EXPECT_EQ(1, subDir4->d_files.size());
    ASSERT_EQ(file->getFileContents(),
              subDir4->d_files["nestdirfile2.txt"]->getFileContents());
}

TEST(NestedDirectoryTest, SymlinkTest)
{
    // unset explicitly
    RECC_PREFIX_REPLACEMENT = {};
    RECC_PROJECT_ROOT = "";

    const std::string cwd =
        FileUtils::get_current_working_directory() + "/symlinkdir";
    digest_string_umap fileMap;
    const std::string subDir = "subdir";
    auto nestedDirectory = make_nesteddirectory(cwd.c_str(), &fileMap, false);

    // because we ingested an absolute path, we need to drill down
    // to the nested directory node that we are interested in
    std::vector<std::string> directories = FileUtils::parseDirectories(cwd);
    auto *subDirectory = &nestedDirectory;
    for (const auto &str : directories) {
        const auto it = subDirectory->d_subdirs->find(str);
        if (it != subDirectory->d_subdirs->end()) {
            subDirectory = &it->second;
        }
    }

    ASSERT_EQ(1, subDirectory->d_subdirs->size());
    ASSERT_EQ(1, subDirectory->d_files.size());
    ASSERT_EQ("regfile data\n",
              fileMap[subDirectory->d_files["regular_file"]->getDigest()]);

    auto subDirectory2 = &(*subDirectory->d_subdirs)[subDir];
    ASSERT_EQ(0, subDirectory2->d_subdirs->size());
    ASSERT_EQ(0, subDirectory2->d_files.size());
    ASSERT_EQ(1, subDirectory2->d_symlinks.size());

    // build a relative path
    // lstat() the file and confirm it's a symlink
    // readlink() the symlink and confirm that the contents
    // are the same as the target in our nested directory structure
    for (const auto &it : subDirectory2->d_symlinks) {
        const std::string name = it.first;
        const std::string target = it.second;
        const std::string path = "symlinkdir/" + subDir + "/" + name;
        struct stat statResult;
        int rc = lstat(path.c_str(), &statResult);
        if (rc != 0) {
            std::cout << "error in lstat() for \"" << path << "\", " << errno
                      << ", " << strerror(errno) << std::endl;
            FAIL();
        }
        ASSERT_TRUE(S_ISLNK(statResult.st_mode));
        std::string contents(statResult.st_size, '\0');
        rc = readlink(path.c_str(), &contents[0], contents.size());
        if (rc < 0) {
            std::cout << "error in readlink() for \"" << path << "\", "
                      << errno << ", " << strerror(errno) << std::endl;
            FAIL();
        }
        ASSERT_EQ(target, contents);
    }
}

TEST(NestedDirectoryTest, EmptyDirTest)
{
    std::string cwd = FileUtils::get_current_working_directory();
    std::string topDir = "nestedtmpdir";
    std::string subDir = "emptyDir";
    std::string dirTree = topDir + "/" + subDir;
    std::string filePath = topDir + "/hello.txt";
    std::string fileContents = "hello!";
    FileUtils::create_directory_recursive((cwd + "/" + dirTree).c_str());
    FileUtils::write_file((cwd + "/" + filePath).c_str(),
                          fileContents.c_str());

    std::string dirPath = cwd + "/" + topDir;
    digest_string_umap fileMap;
    std::cout << dirPath << std::endl;
    auto nestedDirectory = make_nesteddirectory(topDir.c_str(), &fileMap);
    // make sure we don't place directories in filemap
    EXPECT_EQ(fileMap.size(), 1);

    // make sure we also captured the file
    auto nestedtmpdir = nestedDirectory.d_subdirs->begin();
    EXPECT_EQ(nestedtmpdir->second.d_files.size(), 1);

    // should only have one subdirectory
    auto emptydir = nestedtmpdir->second.d_subdirs->begin();
    EXPECT_EQ(emptydir->first, subDir);

    /// sanity check, make sure emptydir is empty
    EXPECT_EQ(emptydir->second.d_files.size(), 0);
    EXPECT_EQ(emptydir->second.d_subdirs->size(), 0);
    EXPECT_EQ(emptydir->second.d_symlinks.size(), 0);

    const std::vector<std::string> rmCommand = {"rm", "-rf", dirPath};
    execute(rmCommand);
}
