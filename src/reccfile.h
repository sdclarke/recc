// Copyright 2019 Bloomberg Finance L.P
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

#ifndef INCLUDED_RECCFILE
#define INCLUDED_RECCFILE

#include <memory>
#include <protos.h>
#include <string>

namespace BloombergLP {
namespace recc {

/*
 * Represents a single file in the filesystem.
 */
class ReccFile {
  public:
    ReccFile(const std::string &file_path, const std::string &file_name,
             const std::string &contents, const proto::Digest &digest,
             bool executable);
    ReccFile() = delete;
    /**
     * Converts a ReccFile to a proto::FileNode with the given name.
     * Defaults to the file_name taken from the path.
     */
    proto::FileNode getFileNode(const std::string &override_name = "") const;
    proto::Digest getDigest() const;
    const std::string &getFileName() const;
    const std::string &getFilePath() const;
    const std::string &getFileContents() const;
    bool isExecutable() const;

  private:
    const std::string d_filePath;
    const std::string d_fileName;
    const std::string d_fileContents;
    const proto::Digest d_digest;
    bool d_executable;
};

/*
 * Constructs a ReccFile given a path.
 */
class ReccFileFactory {
  public:
    static std::shared_ptr<ReccFile> createFile(const char *path);
    ReccFileFactory() = delete;
};

} // namespace recc
} // namespace BloombergLP

#endif
