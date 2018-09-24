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
#include <logging.h>

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

using namespace std;

namespace BloombergLP {
namespace recc {

File::File(const char *path)
{
    RECC_LOG_VERBOSE("Making File object for " << path);
    executable = is_executable(path);
    digest = make_digest(get_file_contents(path));
    RECC_LOG_VERBOSE("executable: " << (executable ? "yes" : "no"));
    RECC_LOG_VERBOSE("digest: " << digest.ShortDebugString());
    return;
}

proto::FileNode File::to_filenode(string name) const
{
    proto::FileNode result;
    result.set_name(name);
    *result.mutable_digest() = digest;
    result.set_is_executable(executable);
    return result;
}

void NestedDirectory::add(File file, const char *relativePath)
{
    const char *slash = strchr(relativePath, '/');
    if (slash) {
        string subdirKey(relativePath, slash - relativePath);
        if (subdirKey.empty()) {
            this->add(file, slash + 1);
        }
        else {
            (*subdirs)[subdirKey].add(file, slash + 1);
        }
    }
    else {
        files[string(relativePath)] = file;
    }
}

proto::Digest NestedDirectory::to_digest(
    unordered_map<proto::Digest, string> *digestMap) const
{
    // The 'files' and 'subdirs' maps make sure everything is sorted by name
    // thus the iterators will iterate lexicographically

    proto::Directory directoryMessage;
    for (const auto &fileIter : files) {
        *directoryMessage.add_files() =
            fileIter.second.to_filenode(fileIter.first);
    }
    for (const auto &subdirIter : *subdirs) {
        auto subdirNode = directoryMessage.add_directories();
        subdirNode->set_name(subdirIter.first);
        auto subdirDigest = subdirIter.second.to_digest(digestMap);
        *subdirNode->mutable_digest() = subdirDigest;
    }
    auto blob = directoryMessage.SerializeAsString();
    auto digest = make_digest(blob);
    if (digestMap != nullptr) {
        (*digestMap)[digest] = blob;
    }
    return digest;
}

proto::Tree NestedDirectory::to_tree() const
{
    proto::Tree result;
    auto root = result.mutable_root();
    for (const auto &fileIter : files) {
        *root->add_files() = fileIter.second.to_filenode(fileIter.first);
    }
    for (const auto &subdirIter : *subdirs) {
        auto subtree = subdirIter.second.to_tree();
        result.mutable_children()->MergeFrom(subtree.children());
        *result.add_children() = subtree.root();
        auto subdirNode = root->add_directories();
        subdirNode->set_name(subdirIter.first);
        *subdirNode->mutable_digest() = make_digest(subtree.root());
    }
    return result;
}

const auto HASH_ALGORITHM = EVP_sha256();
const unsigned char HEX_DIGITS[] = "0123456789abcdef";

proto::Digest make_digest(string blob)
{
    proto::Digest result;

    // Calculate the hash.
    auto hashContext = EVP_MD_CTX_create();
    EVP_DigestInit(hashContext, HASH_ALGORITHM);
    EVP_DigestUpdate(hashContext, &blob[0], blob.length());

    // Store the hash in a char array.
    int hashSize = EVP_MD_size(HASH_ALGORITHM);
    unsigned char hash[hashSize];
    EVP_DigestFinal_ex(hashContext, hash, nullptr);
    EVP_MD_CTX_destroy(hashContext);

    //  Convert the hash to hexadecimal.
    string hashHex(hashSize * 2, '\0');
    for (int i = 0; i < hashSize; ++i) {
        hashHex[i * 2] = HEX_DIGITS[hash[i] >> 4];
        hashHex[i * 2 + 1] = HEX_DIGITS[hash[i] & 0xF];
    }

    result.set_hash(hashHex);
    result.set_size_bytes(blob.length());
    return result;
}

NestedDirectory
make_nesteddirectory(const char *path,
                     unordered_map<proto::Digest, string> *fileMap)
{
    RECC_LOG_VERBOSE("Making NestedDirectory for " << path);
    NestedDirectory result;
    auto dir = opendir(path);
    if (dir == NULL) {
        throw system_error(errno, system_category());
    }

    string pathString(path);
    for (auto dirent = readdir(dir); dirent != nullptr;
         dirent = readdir(dir)) {
        if (strcmp(dirent->d_name, ".") == 0 ||
            strcmp(dirent->d_name, "..") == 0) {
            continue;
        }
        string entityName(dirent->d_name);
        string entityPath = pathString + "/" + entityName;

        struct stat statResult;
        if (stat(entityPath.c_str(), &statResult) != 0) {
            RECC_LOG_VERBOSE("Could not stat " << entityPath << ", skipping");
            continue;
        }
        if (S_ISDIR(statResult.st_mode)) {
            (*result.subdirs)[entityName] =
                make_nesteddirectory(entityPath.c_str(), fileMap);
        }
        else {
            File file(entityPath.c_str());
            result.files[entityName] = file;
            if (fileMap != nullptr) {
                (*fileMap)[file.digest] = entityPath;
            }
        }
    }
    closedir(dir);
    return result;
}

} // namespace recc
} // namespace BloombergLP
