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
//

#include <authsession.h>
#include <fileutils.h>
#include <jsonfilemanager.h>

#include <fstream>
#include <sstream>

namespace BloombergLP {
namespace recc {

JsonFileManager::JsonFileManager(const std::string &jsonPath)
{
    d_jsonFilePath = expand_path(jsonPath);
}

JsonFileManager::~JsonFileManager()
{
    if (d_jsonFileWrite.is_open()) {
        d_jsonFileWrite.close();
    }
    if (d_jsonFileRead.is_open()) {
        d_jsonFileRead.close();
    }
}

void JsonFileManager::write(std::string jsonString)
{
    d_jsonFileWrite.open(d_jsonFilePath);

    if (d_jsonFileWrite.is_open()) {
        d_jsonFileWrite << jsonString;
        d_jsonFileWrite.close();
    }
    else
        throw_error();
}

std::string JsonFileManager::read()
{
    d_jsonFileRead.open(d_jsonFilePath);

    std::ostringstream buffer;
    if (d_jsonFileRead.is_open()) {
        buffer << d_jsonFileRead.rdbuf();
        d_jsonFileRead.close();
    }
    else
        throw_error();
    return buffer.str();
}

void JsonFileManager::throw_error()
{
    const std::string errMsg =
        JwtErrorUtil::error_to_string(d_jsonFilePath, JwtError::NotExist);
    throw std::runtime_error(errMsg);
}

} // namespace recc
} // namespace BloombergLP
