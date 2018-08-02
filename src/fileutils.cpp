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

#include <fileutils.h>

#include <logging.h>
#include <subprocess.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

using namespace std;

namespace BloombergLP {
namespace recc {

TemporaryDirectory::TemporaryDirectory(const char *prefix)
{
    _name = TMPDIR + "/" + string(prefix) + "XXXXXX";
    if (mkdtemp(&_name[0]) == nullptr) {
        throw system_error(errno, system_category());
    }
}

TemporaryDirectory::~TemporaryDirectory()
{
    vector<string> rmCommand = {"rm", "-rf", _name};
    execute(rmCommand);
}

void create_directory_recursive(const char *path)
{
    RECC_LOG_VERBOSE("Creating directory at " << path);
    if (mkdir(path, 0777) != 0) {
        if (errno == EEXIST) {
            // The directory already exists, so return.
            return;
        }
        else if (errno == ENOENT) {
            auto lastSlash = strrchr(path, '/');
            if (lastSlash == nullptr) {
                throw system_error(errno, system_category());
            }
            string parent(path, lastSlash - path);
            create_directory_recursive(parent.c_str());
            if (mkdir(path, 0777) != 0) {
                throw system_error(errno, system_category());
            }
        }
        else {
            throw system_error(errno, system_category());
        }
    }
}

bool is_executable(const char *path)
{
    struct stat statResult;
    if (stat(path, &statResult) == 0) {
        return statResult.st_mode & S_IXUSR;
    }
    throw system_error(errno, system_category());
}

void make_executable(const char *path)
{
    struct stat statResult;
    if (stat(path, &statResult) == 0) {
        if (chmod(path, statResult.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) ==
            0) {
            return;
        }
    }
    throw system_error(errno, system_category());
}

string get_file_contents(const char *path)
{
    string contents;
    ifstream fileStream;
    fileStream.exceptions(ifstream::failbit | ifstream::badbit);
    fileStream.open(path, ios::in | ios::binary);
    auto start = fileStream.tellg();
    fileStream.seekg(0, ios::end);
    auto size = fileStream.tellg() - start;
    RECC_LOG_VERBOSE("Reading file at " << path << " - size " << size);
    contents.resize(size);
    fileStream.seekg(start);
    if (fileStream) {
        fileStream.read(&contents[0], contents.length());
        return contents;
    }
}

void write_file(const char *path, std::string contents)
{
    ofstream fileStream(path, ios::trunc | ios::binary);
    if (!fileStream) {
        auto slash = strrchr(path, '/');
        if (slash != nullptr) {
            string slashPath(path, slash - path);
            slashPath = normalize_path(slashPath.c_str());
            create_directory_recursive(slashPath.c_str());
            fileStream.open(path, ios::trunc | ios::binary);
        }
    }
    fileStream.exceptions(ofstream::failbit | ofstream::badbit);
    fileStream << contents;
}

string normalize_path(const char *path)
{
    vector<string> segments;
    bool global = path[0] == '/';
    while (path[0] != '\0') {
        const char *slash = strchr(path, '/');
        string segment;
        if (slash == nullptr) {
            segment = string(path);
        }
        else {
            segment = string(path, slash - path);
        }
        if (segment == ".." && !segments.empty() && segments.back() != "..") {
            segments.pop_back();
        }
        else if (segment != "." && segment != "") {
            segments.push_back(segment);
        }
        if (slash == nullptr) {
            break;
        }
        else {
            path = slash + 1;
        }
    }

    string result(global ? "/" : "");
    if (segments.size() > 0) {
        for (const auto &segment : segments) {
            result += segment + "/";
        }
        result.pop_back();
    }
    return result;
}
} // namespace recc
} // namespace BloombergLP
