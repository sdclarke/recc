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

#include <fileutils.h>
#include <logging.h>
#include <merklize.h>
#include <reccfile.h>

namespace BloombergLP {
namespace recc {
ReccFile::ReccFile(const std::string &file_path, const std::string &file_name,
                   const std::string &contents, const proto::Digest &digest,
                   bool executable)
    : d_filePath(file_path), d_fileName(file_name), d_fileContents(contents),
      d_digest(digest), d_executable(executable)
{
}

proto::FileNode ReccFile::getFileNode(const std::string &override_name) const
{
    if (override_name.empty() && d_fileName.empty()) {
        throw std::logic_error("Empty name passed to filenode, and class "
                               "variable file_name is empty.");
    }
    proto::FileNode result;
    result.set_name(override_name.empty() ? d_fileName : override_name);
    *result.mutable_digest() = d_digest;
    result.set_is_executable(d_executable);
    return result;
}

proto::Digest ReccFile::getDigest() const { return d_digest; }

const std::string &ReccFile::getFileName() const { return d_fileName; }

const std::string &ReccFile::getFilePath() const { return d_filePath; }

const std::string &ReccFile::getFileContents() const { return d_fileContents; }

bool ReccFile::isExecutable() const { return d_executable; }

std::shared_ptr<ReccFile> ReccFileFactory::createFile(const char *path)
{
    if (path != nullptr) {
        bool executable = is_executable(path);
        const std::string file_name = path_basename(path);
        const std::string file_contents = get_file_contents(path);
        proto::Digest file_digest = make_digest(file_contents);

        RECC_LOG_VERBOSE("Creating"
                         << (executable ? " " : " non-")
                         << "executable file object"
                         << " with digest: " << file_digest.ShortDebugString()
                         << " and path: " << path);

        return std::make_shared<ReccFile>(ReccFile(std::string(path),
                                                   file_name, file_contents,
                                                   file_digest, executable));
    }
    else {
        RECC_LOG_ERROR("Path is not valid");
        return nullptr;
    }
}

} // namespace recc
} // namespace BloombergLP