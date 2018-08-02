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

#include <subprocess.h>

#include <cerrno>
#include <cstring>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

using namespace std;

namespace BloombergLP {
namespace recc {

SubprocessResult execute(vector<string> command, bool pipeStdOut,
                         bool pipeStdErr, map<string, string> env,
                         const char *cwd)
{
    // Convert the command to a char*[]
    int argc = command.size();
    const char *argv[argc + 1];
    for (int i = 0; i < argc; ++i) {
        argv[i] = command[i].c_str();
    }
    argv[argc] = nullptr;

    // Pipe, fork and exec
    int stdOutPipeFDs[2] = {0};
    if (pipeStdOut && pipe(stdOutPipeFDs) == -1) {
        throw system_error(errno, system_category());
    }
    int stdErrPipeFDs[2] = {0};
    if (pipeStdErr && pipe(stdErrPipeFDs) == -1) {
        throw system_error(errno, system_category());
    }
    auto pid = fork();
    if (pid == -1) {
        throw system_error(errno, system_category());
    }
    else if (pid == 0) {
        // (runs only in the child)
        if (pipeStdOut) {
            close(stdOutPipeFDs[0]);
            dup2(stdOutPipeFDs[1],
                 STDOUT_FILENO); // redirect stdout to input end of pipe
            close(stdOutPipeFDs[1]);
        }
        if (pipeStdErr) {
            close(stdErrPipeFDs[0]);
            dup2(stdErrPipeFDs[1],
                 STDERR_FILENO); // redirect stderr to input end of pipe
            close(stdErrPipeFDs[1]);
        }
        if (cwd != nullptr) {
            if (chdir(cwd) == -1) {
                perror(nullptr);
                _Exit(1);
            }
        }
        for (auto &envPair : env) {
            setenv(envPair.first.c_str(), envPair.second.c_str(), 1);
        }
        execvp(argv[0], const_cast<char *const *>(argv));
        perror(nullptr);
        _Exit(1);
    }
    // (runs only in the parent)
    SubprocessResult result;

    // Get the output from the child process
    fd_set fdSet;
    FD_ZERO(&fdSet);
    if (pipeStdOut) {
        FD_SET(stdOutPipeFDs[0], &fdSet);
        close(stdOutPipeFDs[1]);
    }
    if (pipeStdErr) {
        FD_SET(stdErrPipeFDs[0], &fdSet);
        close(stdErrPipeFDs[1]);
    }
    char buffer[4096];
    bool hasMoreData = true;
    while (FD_ISSET(stdOutPipeFDs[0], &fdSet) ||
           FD_ISSET(stdErrPipeFDs[0], &fdSet)) {
        fd_set readFDSet = fdSet;
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        select(FD_SETSIZE, &readFDSet, nullptr, nullptr, &timeout);
        if (FD_ISSET(stdOutPipeFDs[0], &readFDSet)) {
            int bytesRead = read(stdOutPipeFDs[0], buffer, sizeof(buffer));
            if (bytesRead > 0) {
                result.stdOut.append(buffer, bytesRead);
            }
            else {
                close(stdOutPipeFDs[0]);
                FD_CLR(stdOutPipeFDs[0], &fdSet);
            }
        }
        if (FD_ISSET(stdErrPipeFDs[0], &readFDSet)) {
            int bytesRead = read(stdErrPipeFDs[0], buffer, sizeof(buffer));
            if (bytesRead > 0) {
                result.stdErr.append(buffer, bytesRead);
            }
            else {
                close(stdErrPipeFDs[0]);
                FD_CLR(stdErrPipeFDs[0], &fdSet);
            }
        }
    }

    // Get the status code from the child process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        throw system_error(errno, system_category());
    }
    result.exitCode = WEXITSTATUS(status);
    return result;
}
} // namespace recc
} // namespace BloombergLP
