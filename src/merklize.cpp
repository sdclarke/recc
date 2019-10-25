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

#include <digestgenerator.h>
#include <fileutils.h>
#include <logging.h>
#include <reccmetrics/metricguard.h>
#include <reccmetrics/totaldurationmetrictimer.h>

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <memory>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

#define TIMER_NAME_CALCULATE_DIGESTS_TOTAL "recc.calculate_digests_total"

namespace BloombergLP {
namespace recc {

void NestedDirectory::add(std::shared_ptr<ReccFile> file,
                          const char *relativePath, bool checkedPrefix)
{
    // A forward slash by itself is not a valid input path
    if (relativePath == nullptr || strcmp(relativePath, "/") == 0) {
        return;
    }

    std::string replacedDirectory(relativePath);
    // Check if directory passed in matches any in PREFIX_REPLACEMENT_MAP. if
    // so replace the path. Only check on the inital call, when the full
    // directory path is avaliable.
    if (!checkedPrefix) {
        replacedDirectory =
            FileUtils::resolve_path_from_prefix_map(std::string(relativePath));
        checkedPrefix = true;
    }

    const char *slash = strchr(replacedDirectory.c_str(), '/');
    if (slash) {
        const std::string subdirKey(
            replacedDirectory.c_str(),
            static_cast<size_t>(slash - replacedDirectory.c_str()));

        if (subdirKey.empty()) {
            this->add(file, slash + 1, checkedPrefix);
        }
        else {
            (*d_subdirs)[subdirKey].add(file, slash + 1, checkedPrefix);
        }
    }
    else {
        d_files[replacedDirectory] = file;
    }
}

void NestedDirectory::addDirectory(const char *directory, bool checkedPrefix)
{
    // A forward slash by itself is not a valid input directory
    if (directory == nullptr || strcmp(directory, "/") == 0) {
        return;
    }

    // If an absolute path, go one past the first slash, saving an unnecessary
    // recursive call.
    if (directory[0] == '/') {
        directory++;
    }

    std::string replacedDirectory(directory);
    // Check if directory passed in matches any in PREFIX_REPLACEMENT_MAP. if
    // so replace the path. Only check on the initial call, when the full
    // directory path is available.
    if (!checkedPrefix) {
        replacedDirectory =
            FileUtils::resolve_path_from_prefix_map(std::string(directory));
        checkedPrefix = true;
    }

    // Find first occurrence of character in the string
    const char *slash = strchr(replacedDirectory.c_str(), '/');
    if (slash) {
        // Construct string before '/' - the parent directory
        const std::string subdirKey(
            replacedDirectory.c_str(),
            static_cast<size_t>(slash - replacedDirectory.c_str()));

        // If no parent directory, add the subdirectory
        if (subdirKey.empty()) {
            this->addDirectory(slash + 1, checkedPrefix);
        }
        // If parent directory, map it to subdirectories
        else {
            (*d_subdirs)[subdirKey].addDirectory(slash + 1, checkedPrefix);
        }
    }
    // If directory doesn't exist in our map, add mapping to a new
    // NestedDirectory
    else {
        if (d_subdirs->count(replacedDirectory) == 0) {
            d_subdirs->emplace(replacedDirectory, NestedDirectory());
        }
    }
}

proto::Digest NestedDirectory::to_digest(digest_string_umap *digestMap) const
{
    // The 'd_files' and 'd_subdirs' maps make sure everything is sorted by
    // name thus the iterators will iterate lexicographically

    proto::Directory directoryMessage;
    for (const auto &fileIter : d_files) {
        *directoryMessage.add_files() =
            fileIter.second->getFileNode(fileIter.first);
    }

    for (const auto &subdirIter : *d_subdirs) {
        auto subdirNode = directoryMessage.add_directories();
        subdirNode->set_name(subdirIter.first);
        auto subdirDigest = subdirIter.second.to_digest(digestMap);
        *subdirNode->mutable_digest() = subdirDigest;
    }

    const auto blob = directoryMessage.SerializeAsString();
    const auto digest = DigestGenerator::make_digest(blob);

    if (digestMap != nullptr) {
        digestMap->emplace(digest, blob);
    }

    return digest;
}

/**
 * Helper method, iterates through local filesystem, and populates fileMap,
 * and filePathMap.
 */
void make_nesteddirectoryhelper(
    const char *path, digest_string_umap *fileMap,
    std::unordered_map<std::shared_ptr<ReccFile>, std::string> *filePathMap)
{
    RECC_LOG_VERBOSE("NestedDirectoryHelper, Iterating through " << path);

    // dir is used to iterate through subdirectories in path
    auto dir = opendir(path);
    if (dir == nullptr) {
        throw std::system_error(errno, std::system_category());
    }

    // pathString is used to keep track of the top-level directory
    const std::string pathString(path);
    // Iterate through directory entries, if a directory, recurse, otherwise
    // (a file) store file digest -> path in fileMap passed in.
    for (auto dirent = readdir(dir); dirent != nullptr;
         dirent = readdir(dir)) {
        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0) {
            continue;
        }

        const std::string entityName(dirent->d_name);
        const std::string entityPath = pathString + "/" + entityName;

        struct stat statResult;
        if (stat(entityPath.c_str(), &statResult) != 0) {
            RECC_LOG_VERBOSE("Could not stat [" << entityPath
                                                << "] skipping.");
            continue;
        }

        if (S_ISDIR(statResult.st_mode)) {
            make_nesteddirectoryhelper(entityPath.c_str(), fileMap,
                                       filePathMap);
        }
        else {
            const std::shared_ptr<ReccFile> file =
                ReccFileFactory::createFile(entityPath.c_str());

            if (fileMap != nullptr) {
                // If the path matches any in RECC_PATH_PREFIX, replace it if
                // necessary, and normalize path.
                const std::string replacedRoot =
                    FileUtils::resolve_path_from_prefix_map(entityPath)
                        .c_str();

                // Get the relativePath from the current PROJECT_ROOT.
                const std::string relativePath = FileUtils::make_path_relative(
                    replacedRoot, RECC_PROJECT_ROOT.c_str());

                // Normalize path
                const std::string normalizedReplacedRoot =
                    FileUtils::normalize_path(relativePath.c_str());

                RECC_LOG_VERBOSE("Mapping local file path: ["
                                 << entityPath
                                 << "] to normalized-relative (if)updated: ["
                                 << normalizedReplacedRoot << "]");

                // Store the digest, and the file contents.
                fileMap->emplace(file->getDigest(), file->getFileContents());
                // Store the updated/replaced path in the filePathMap, which
                // will be used to construct the NestedDirectory later.
                filePathMap->emplace(file, normalizedReplacedRoot);
            }
        }
    }
    closedir(dir);
}

NestedDirectory make_nesteddirectory(const char *path,
                                     digest_string_umap *fileMap)
{
    NestedDirectory nestedDir;
    std::unordered_map<std::shared_ptr<ReccFile>, std::string> file_path_map;

    // Populate both maps
    make_nesteddirectoryhelper(path, fileMap, &file_path_map);

    // Construct nestedDirectory
    for (const auto &file : file_path_map) {
        nestedDir.add(file.first, file.second.c_str());
    }

    return nestedDir;
}

} // namespace recc
} // namespace BloombergLP
