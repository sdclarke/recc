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

#include <buildboxcommon_fileutils.h>
#include <env.h>
#include <logging.h>
#include <protos.h>
#include <reccfile.h>

#include <google/protobuf/message.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace BloombergLP {
namespace recc {

typedef std::unordered_map<proto::Digest, std::string> digest_string_umap;

/**
 * Represents a directory that, optionally, has other directories inside.
 */
struct NestedDirectory {
    // Important to use a sorted map to keep subdirectories ordered by name
    typedef std::map<std::string, NestedDirectory> subdir_map;

    std::unique_ptr<subdir_map> d_subdirs;
    // Important to use a sorted map to keep files ordered by name
    std::map<std::string, std::shared_ptr<ReccFile>> d_files;

    // name, target
    std::map<std::string, std::string> d_symlinks;

    NestedDirectory() : d_subdirs(std::make_unique<subdir_map>()){};

    /**
     * Add the given File to this NestedDirectory at the given relative path,
     * which may include subdirectories.
     * checkedPrefix is used to test whether this is the first recursive call,
     * and if so check the prefix.
     */
    void add(std::shared_ptr<ReccFile> file, const char *relativePath,
             bool checkedPrefix = false);

    /**
     * Add the given symlink to this NestedDirectory at the given relative
     * path, which may include subdirectories
     */
    void addSymlink(const std::string &target, const char *relativePath,
                    bool checkedPrefix = false);

    /**
     * Add the given Directory to this NestedDirectory at a given relative
     * path. If the directory has contents, the add method should be used
     * instead.
     * checkedPrefix is used to test whether this is the first recursive
     * call, and if so check the prefix.
     */
    void addDirectory(const char *directory, bool checkedPrefix = false);

    /**
     * Convert this NestedDirectory to a Directory message and return its
     * Digest.
     *
     * If a digestMap is passed, serialized Directory messages
     * corresponding to this directory and its subdirectories will be
     * stored in it using their Digest messages as the keys. (This is
     * recursive -- nested subdirectories will also be stored.
     */
    proto::Digest to_digest(digest_string_umap *digestMap = nullptr) const;

    void print(std::ostream &out, const std::string &dirName = "") const;
};

/**
 * Create a NestedDirectory containing the contents of the given path and
 * its subdirectories.
 *
 * If a fileMap is passed, paths to all files referenced by the
 * NestedDirectory will be stored in it using their Digest messages as the
 * keys.
 *
 * CheckedPrefix is used to test whether this is the first recursive
 * call, and if so check the prefix.
 */
NestedDirectory make_nesteddirectory(const char *path,
                                     digest_string_umap *fileMap = nullptr,
                                     const bool followSymlinks = true);

std::ostream &operator<<(std::ostream &out, const NestedDirectory &obj);

} // namespace recc
} // namespace BloombergLP

#endif
