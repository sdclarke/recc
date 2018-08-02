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

#ifndef INCLUDED_MERKLIZE
#define INCLUDED_MERKLIZE

#include <protos.h>

#include <google/protobuf/message.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace BloombergLP {
namespace recc {

/**
 * Represents a single file.
 */
struct File {
    proto::Digest digest;
    bool executable;

    File(){};

    File(proto::Digest digest, bool executable)
        : digest(digest), executable(executable){};

    /**
     * Constructs a File given the path to a file on disk.
     */
    File(const char *path);

    /**
     * Converts a File to a FileNode with the given name.
     */
    proto::FileNode to_filenode(std::string name) const;
};

/**
 * Represents a directory that, optionally, has other directories inside.
 */
struct NestedDirectory {
    typedef std::map<std::string, NestedDirectory> subdir_map;

    std::unique_ptr<subdir_map> subdirs;
    std::map<std::string, File> files;

    NestedDirectory() : subdirs(new subdir_map){};

    /**
     * Add the given File to this NestedDirectory at the given relative path,
     * which may include subdirectories.
     */
    void add(File, const char *relativePath);

    /**
     * Convert this NestedDirectory to a Directory message and return its
     * Digest.
     *
     * If a digestMap is passed, serialized Directory messages corresponding to
     * this directory and its subdirectories will be stored in it using their
     * Digest messages as the keys. (This is recursive -- nested subdirectories
     * will also be stored.
     */
    proto::Digest to_digest(std::unordered_map<proto::Digest, std::string>
                                *digestMap = nullptr) const;

    /**
     * Convert this NestedDirectory to a Tree message.
     */
    proto::Tree to_tree() const;
};

/**
 * Create a Digest message from the given blob.
 */
proto::Digest make_digest(std::string blob);

/**
 * Create a Digest message from the given proto message.
 */
inline proto::Digest make_digest(const google::protobuf::MessageLite &message)
{
    return make_digest(message.SerializeAsString());
}

/**
 * Create a NestedDirectory containing the contents of the given path and its
 * subdirectories.
 *
 * If a fileMap is passed, paths to all files referenced by the NestedDirectory
 * will be stored in it using their Digest messages as the keys.
 */
NestedDirectory make_nesteddirectory(
    const char *path,
    std::unordered_map<proto::Digest, std::string> *fileMap = nullptr);

} // namespace recc
} // namespace BloombergLP

#endif
