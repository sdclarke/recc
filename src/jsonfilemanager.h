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

#ifndef INCLUDED_JSONFILEMANAGER
#define INCLUDED_JSONFILEMANAGER

#include <fstream>
#include <string>

namespace BloombergLP {
namespace recc {

/**
 * A class which manages file objects
 */
class JsonFileManager {
  public:
    /**
     *  Sets d_jsonFilePath
     */
    JsonFileManager(const std::string &jsonPath);

    /**
     * Closes any file streams which are still open
     */
    ~JsonFileManager();

    /**
     * Write jsonString to d_jsonFilePath.
     * Will overwrite contents initially on file.
     * Opens and closes file stream in function call.
     */
    void write(std::string jsonString);

    /**
     * Return contents in d_jsonFilePath.
     * Opens and closes file stream in function call.
     */
    std::string read();

  private:
    /**
     * Throw an error if file isnt good
     */
    void throw_error();
    std::ifstream d_jsonFileRead;
    std::ofstream d_jsonFileWrite;
    std::string d_jsonFilePath;
};

} // namespace recc
} // namespace BloombergLP

#endif
