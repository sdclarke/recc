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

#include <env.h>
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

namespace BloombergLP {
namespace recc {

TemporaryDirectory::TemporaryDirectory(const char *prefix)
{
    d_name = TMPDIR + "/" + std::string(prefix) + "XXXXXX";
    if (mkdtemp(&d_name[0]) == nullptr) {
        throw std::system_error(errno, std::system_category());
    }
}

TemporaryDirectory::~TemporaryDirectory()
{
    const std::vector<std::string> rmCommand = {"rm", "-rf", d_name};
    // TODO: catch here so as to not throw from destructor
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
                throw std::system_error(errno, std::system_category());
            }
            std::string parent(path, lastSlash - path);
            create_directory_recursive(parent.c_str());
            if (mkdir(path, 0777) != 0) {
                throw std::system_error(errno, std::system_category());
            }
        }
        else {
            throw std::system_error(errno, std::system_category());
        }
    }
}

bool is_executable(const char *path)
{
    struct stat statResult;
    if (stat(path, &statResult) == 0) {
        return statResult.st_mode & S_IXUSR;
    }
    throw std::system_error(errno, std::system_category());
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
    throw std::system_error(errno, std::system_category());
}

std::string get_file_contents(const char *path)
{
    std::string contents;
    std::ifstream fileStream;
    fileStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fileStream.open(path, std::ios::in | std::ios::binary);
    auto start = fileStream.tellg();
    fileStream.seekg(0, std::ios::end);
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
    std::ofstream fileStream(path, std::ios::trunc | std::ios::binary);
    if (!fileStream) {
        auto slash = strrchr(path, '/');
        if (slash != nullptr) {
            std::string slashPath(path, slash - path);
            slashPath = normalize_path(slashPath.c_str());
            create_directory_recursive(slashPath.c_str());
            fileStream.open(path, std::ios::trunc | std::ios::binary);
        }
    }
    fileStream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    fileStream << contents;
}

std::string normalize_path(const char *path)
{
    std::vector<std::string> segments;
    bool global = path[0] == '/';
    while (path[0] != '\0') {
        const char *slash = strchr(path, '/');
        std::string segment;
        if (slash == nullptr) {
            segment = std::string(path);
        }
        else {
            segment = std::string(path, slash - path);
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

    std::string result(global ? "/" : "");
    if (segments.size() > 0) {
        for (const auto &segment : segments) {
            result += segment + "/";
        }
        result.pop_back();
    }
    return result;
}

bool has_path_prefix(const std::string &path, std::string prefix)
{
    /* A path can never have the empty path as a prefix */
    if (prefix.empty()) {
        return false;
    }

    /*
     * Make sure prefix ends in a slash.
     * This is so we don't return true if path = /foo and prefix = /foobar
     */
    if (prefix.back() != '/') {
        prefix.push_back('/');
    }

    return path.substr(0, prefix.length()) == prefix;
}

std::string make_path_relative(std::string path, const char *workingDirectory)
{
    if (workingDirectory == nullptr || workingDirectory[0] == 0 ||
        path.length() == 0 || path[0] != '/' ||
        !has_path_prefix(path, RECC_PROJECT_ROOT)) {
        return path;
    }
    if (workingDirectory[0] != '/') {
        throw std::logic_error(
            "Working directory must be null or an absolute path");
    }

    int i = 0;
    int lastMatchingSegmentEnd = 0;
    while (i < path.length() && path[i] == workingDirectory[i]) {
        if (workingDirectory[i + 1] == 0) {
            // Working directory is prefix of path, so if the last
            // segment matches, we're done.
            if (path.length() == i + 1) {
                return std::string(path[i] == '/' ? "./" : ".");
            }
            else if (path.length() == i + 2 && path[i + 1] == '/') {
                return std::string("./");
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
            return std::string(".");
        }
        else {
            lastMatchingSegmentEnd = i;
            ++i;
        }
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

std::string make_path_absolute(const std::string &path, const std::string &cwd)
{
    if (path.empty() || path.front() == '/') {
        return path;
    }
    const std::string fullPath = cwd + '/' + path;
    std::string normalizedPath = normalize_path(fullPath.c_str());

    /* normalize_path removes trailing slashes, so let's preserve them here */
    if (path.back() == '/' && normalizedPath.back() != '/') {
        normalizedPath.push_back('/');
    }
    return normalizedPath;
}

std::string get_current_working_directory()
{
    int bufferSize = 1024;
    while (true) {
        char buffer[bufferSize];
        char *cwd = getcwd(buffer, bufferSize);
        if (cwd != nullptr) {
            return std::string(cwd);
        }
        else if (errno == ERANGE) {
            bufferSize *= 2;
        }
        else {
            RECC_LOG_PERROR(
                "Warning: could not get current working directory");
            return std::string();
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
        const int segmentLength = slash - path;
        if (segmentLength == 0 || (segmentLength == 1 && path[0] == '.')) {
            // Empty or dot segments don't change the level.
        }
        else if (segmentLength == 2 && path[0] == '.' && path[1] == '.') {
            currentLevel--;
            lowestLevel = std::min(lowestLevel, currentLevel);
        }
        else {
            currentLevel++;
        }
        path = slash + 1;
    }
    if (strcmp(path, "..") == 0) {
        currentLevel--;
        lowestLevel = std::min(lowestLevel, currentLevel);
    }
    return -lowestLevel;
}

std::string last_n_segments(const char *path, int n)
{
    if (n == 0)
        return std::string();
    const int pathLength = strlen(path);
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
                return std::string(substringStart, substringLength);
            }
        }
        substringStart--;
        substringLength++;
    }
    // The path might only be one segment (no slashes)
    if (slashesSeen == 0 && n == 1) {
        return std::string(path, pathLength);
    }
    throw std::logic_error("Not enough segments in path");
}

} // namespace recc
} // namespace BloombergLP
