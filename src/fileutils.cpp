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

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

#include <unistd.h>

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
    }
    return contents;
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

string make_path_relative(string path, const char *workingDirectory)
{
    if (workingDirectory == nullptr || workingDirectory[0] == 0 ||
        path.length() == 0 || path[0] != '/') {
        return path;
    }
    if (workingDirectory[0] != '/') {
        throw logic_error(
            "Working directory must be null or an absolute path");
    }

    int i = 0;
    int lastMatchingSegmentEnd = 0;
    while (i < path.length() && path[i] == workingDirectory[i]) {
        if (workingDirectory[i + 1] == 0) {
            // Working directory is prefix of path, so if the last
            // segment matches, we're done.
            if (path.length() == i + 1) {
                return string(path[i] == '/' ? "./" : ".");
            }
            else if (path.length() == i + 2 && path[i + 1] == '/') {
                return string("./");
            }
            else if (path[i] == '/') {
                return path.substr(i + 1);
            }
            else if (path[i + 1] == '/') {
                return path.substr(i + 2);
            }
        }
        else if (path[i] == '/') {
            lastMatchingSegmentEnd = i;
        }
        ++i;
    }

    if (i == path.length() && workingDirectory[i] == '/') {
        // Path is prefix of working directory.
        if (workingDirectory[i + 1] == 0) {
            return string(".");
        }
        else {
            lastMatchingSegmentEnd = i;
            ++i;
        }
    }

    if (lastMatchingSegmentEnd == 0) {
        // The path and the working directory have nothing in common,
        // so we assume the path is a system folder like /usr/include
        // and not a build input.
        return path;
    }

    int dotdotsNeeded = 1;
    while (workingDirectory[i] != 0) {
        if (workingDirectory[i] == '/' && workingDirectory[i + 1] != 0) {
            ++dotdotsNeeded;
        }
        ++i;
    }
    auto result =
        path.replace(0, lastMatchingSegmentEnd, dotdotsNeeded * 3 - 1, '.');
    for (int j = 0; j < dotdotsNeeded - 1; ++j) {
        result[j * 3 + 2] = '/';
    }
    return result;
}

string get_current_working_directory()
{
    int bufferSize = 1024;
    while (true) {
        char buffer[bufferSize];
        char *cwd = getcwd(buffer, bufferSize);
        if (cwd != nullptr) {
            return string(cwd);
        }
        else if (errno == ERANGE) {
            bufferSize *= 2;
        }
        else {
            perror("Warning: could not get current working directory");
            return string();
        }
    }
}

int parent_directory_levels(const char *path)
{
    int currentLevel = 0;
    int lowestLevel = 0;
    while (*path != 0) {
        const char *slash = strchr(path, '/');
        if (!slash)
            break;
        int segmentLength = slash - path;
        if (segmentLength == 0 || (segmentLength == 1 && path[0] == '.')) {
            // Empty or dot segments don't change the level.
        }
        else if (segmentLength == 2 && path[0] == '.' && path[1] == '.') {
            currentLevel--;
            lowestLevel = min(lowestLevel, currentLevel);
        }
        else {
            currentLevel++;
        }
        path = slash + 1;
    }
    if (strcmp(path, "..") == 0) {
        currentLevel--;
        lowestLevel = min(lowestLevel, currentLevel);
    }
    return -lowestLevel;
}

string last_n_segments(const char *path, int n)
{
    if (n == 0)
        return string();
    int pathLength = strlen(path);
    const char *substringStart = path + pathLength - 1;
    int substringLength = 1;
    int slashesSeen = 0;
    if (path[pathLength - 1] == '/') {
        substringLength = 0;
    }
    while (substringStart != path) {
        if (*(substringStart - 1) == '/') {
            slashesSeen++;
            if (slashesSeen == n) {
                return string(substringStart, substringLength);
            }
        }
        substringStart--;
        substringLength++;
    }
    throw logic_error("Not enough segments in path");
}

} // namespace recc
} // namespace BloombergLP
