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

#ifndef INCLUDED_SUBPROCESS
#define INCLUDED_SUBPROCESS

#include <map>
#include <string>
#include <vector>

namespace BloombergLP {
namespace recc {

/**
 * Represents the result of executing a subprocess.
 */
struct SubprocessResult {
    int exitCode;
    std::string stdOut; // Only valid if pipeStdOut was true
    std::string stdErr; // Only valid if pipeStdErr was true
};

/**
 * Execute the given command, returning a SubprocessResult.
 *
 * If pipeStdOut is true, standard output will be redirected and returned as
 * part of the SubprocessResult. Similarly, pipeStdErr will redirect the
 * standard error stream.
 *
 * The keys and values in env will be added to the given process's environment.
 *
 * If cwd is non-empty, it specifies the current working directory of the
 * subprocess.
 */
SubprocessResult execute(std::vector<std::string> command,
                         bool pipeStdOut = false, bool pipeStdErr = false,
                         std::map<std::string, std::string> env = {},
                         const char *cwd = nullptr);
} // namespace recc
} // namespace BloombergLP

#endif
