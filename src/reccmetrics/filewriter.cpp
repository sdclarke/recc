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

#include <stdexcept>

#include <reccmetrics/filewriter.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {

FileWriter::FileWriter(const std::string &filePath) { openFile(filePath); }

void FileWriter::openFile(const std::string &filePath)
{
    d_of.open(filePath, std::ofstream::out | std::ofstream::app);
    if (!d_of.good()) {
        throw std::runtime_error("Cannot open file [" + filePath +
                                 "] for writing.");
    }
}

void FileWriter::write(const std::string &buffer) { d_of << buffer; }

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
