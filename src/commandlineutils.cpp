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

#include <commandlineutils.h>

#include <numeric>

namespace BloombergLP {
namespace recc {

bool CommandLineUtils::starts_with(const std::string &input,
                                   const std::string &prefix)
{
    return input.compare(0, prefix.length(), prefix) == 0;
}

std::vector<std::string> includePrefixes = {
    "-include",   "-imacros", "-I",        "-iquote",   "-isystem",
    "-idirafter", "-iprefix", "-isysroot", "--sysroot="};

std::vector<std::string>
CommandLineUtils::split_option_from_arg(const std::string &optionArg)
{
    for (const std::string &prefix : includePrefixes) {
        if (starts_with(optionArg, prefix) &&
            optionArg.length() > prefix.length()) {
            return {optionArg.substr(0, prefix.length()),
                    optionArg.substr(prefix.length())};
        }
    }
    return {optionArg};
}

std::vector<std::string>
CommandLineUtils::prepend_absolute_paths_in_compile_command(
    std::vector<std::string> commandArgs, const std::string &pathPrefix)
{
    std::vector<std::string> command;
    bool first = true;
    for (const std::string &arg : commandArgs) {
        // Don't rewrite the path to the compiler
        if (first) {
            first = false;
            command.push_back(arg);
            continue;
        }
        // Prepend absolute paths on the command line with the tmpdir.
        auto argTokens = split_option_from_arg(arg);
        for (std::string &argToken : argTokens) {
            if (!argToken.empty() && argToken[0] == '/') {
                argToken = pathPrefix + argToken;
            }
        }
        const std::string newArg = std::accumulate(
            argTokens.begin(), argTokens.end(), std::string(""));
        command.push_back(newArg);
    }
    return command;
}

} // namespace recc
} // namespace BloombergLP
