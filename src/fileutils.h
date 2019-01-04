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

#ifndef INCLUDED_FILEUTILS
#define INCLUDED_FILEUTILS

#include <reccdefaults.h>
#include <string>

namespace BloombergLP {
namespace recc {

class TemporaryDirectory {
  public:
    /**
     * Create a temporary directory on disk. If a prefix is specified, it
     * will be included in the name of the temporary directory.
     */
    TemporaryDirectory(const char *prefix = DEFAULT_RECC_TMP_PREFIX);

    /**
     * Delete the temporary directory.
     */
    ~TemporaryDirectory();

    const char *name() const { return d_name.c_str(); };

  private:
    std::string d_name;
};

/**
 * Create a directory if it doesn't already exist, creating parent directories
 * as needed.
 */
void create_directory_recursive(const char *path);

/**
 * Return true if the given file path is executable.
 */
bool is_executable(const char *path);

/**
 * Make the given file executable.
 */
void make_executable(const char *path);

/**
 * Given the path to a file, return a std::string with its contents.
 *
 * The path must be a path to a file that exists on disk. It can be absolute
 * or relative to the current directory.
 */
std::string get_file_contents(const char *path);

/**
 * Overwrite the given file with the given contents.
 */
void write_file(const char *path, std::string contents);

/**
 * Simplify the given path.
 *
 * The returned path will not contain any empty or `.` segments, and any `..`
 * segments will occur at the start of the path.
 */
std::string normalize_path(const char *path);

/**
 * Make the given path relative to the given working directory.
 *
 * If the given working directory is null, or if the given path has nothing to
 * do with the working directory, the path will be returned unmodified.
 */
std::string make_path_relative(std::string path, const char *workingDirectory);

/**
 * Return the current working directory.
 *
 * If the current working directory cannot be determined (if the user does not
 * have permission to read the current directory, for example), return the
 * empty std::string and log a warning.
 */
std::string get_current_working_directory();

/**
 * Return the number of levels of parent directory needed to follow the given
 * path.
 *
 * For example, "a/b/c.txt" has zero parent directory levels, "a/../../b.txt"
 * has one, and "../.." has two.
 */
int parent_directory_levels(const char *path);

/**
 * Return a std::string containing the last N segments of the given path,
 * without a trailing slash.
 *
 * If the given path doesn't have that many segments, throws an exception.
 */
std::string last_n_segments(const char *path, int n);
} // namespace recc
} // namespace BloombergLP
#endif
