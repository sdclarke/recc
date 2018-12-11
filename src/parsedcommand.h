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

#ifndef INCLUDED_PARSEDCOMMAND
#define INCLUDED_PARSEDCOMMAND

#include <initializer_list>
#include <logging.h>
#include <set>
#include <string>
#include <vector>

namespace BloombergLP {
namespace recc {

/**
 * Convert a null-terminated list of C strings to a vector of C++ strings.
 */
static std::vector<std::string> vector_from_argv(const char *const *argv)
{
    std::vector<std::string> result;
    int i = 0;

    RECC_LOG_VERBOSE("Parsing command: ");

    while (argv[i] != nullptr) {
        std::string argstr = std::string(argv[i]);
        ++i;

        RECC_LOG_VERBOSE("argv[" << i << "] = " << argstr);
        result.push_back(argstr);
    }

    return result;
}

/**
 * Represents the result of parsing a compiler command.
 */
class ParsedCommand {
  public:
    /**
     * Parses the given command. If workingDirectory is non-null, replace
     * absolute paths with paths relative to the given working directory.
     */
    ParsedCommand(std::vector<std::string> command,
                  const char *workingDirectory);
    ParsedCommand(char **argv, const char *workingDirectory)
        : ParsedCommand(vector_from_argv(argv), workingDirectory)
    {
    }
    ParsedCommand(std::initializer_list<std::string> command)
        : ParsedCommand(std::vector<std::string>(command), nullptr)
    {
    }

    /**
     * Returns true if the given command is a supported compiler command.
     */
    bool is_compiler_command() const { return d_compilerCommand; }

    /**
     * Returns the original command that was passed to the constructor, with
     * absolute paths replaced with equivalent relative paths.
     */
    std::vector<std::string> get_command() const { return d_command; }

    /**
     * Return a command that prints this command's dependencies in Makefile
     * format. If this command is not a supported compiler command, the result
     * is undefined.
     */
    std::vector<std::string> get_dependencies_command() const
    {
        return d_dependenciesCommand;
    }

    /**
     * Return the output files specified in the command arguments.
     *
     * This is not necessarily all of the files the command will create. (For
     * example, if no output files are specified, many compilers will write to
     * a.out by default.)
     */
    std::set<std::string> get_products() const { return d_commandProducts; }

    /**
     * If true, the dependencies command will produce nonstandard Sun-style
     * make rules where one dependency is listed per line and spaces aren't
     * escaped.
     */
    bool produces_sun_make_rules() const { return d_producesSunMakeRules; }

  private:
    bool d_compilerCommand;
    std::vector<std::string> d_command;
    std::vector<std::string> d_dependenciesCommand;
    std::set<std::string> d_commandProducts;
    bool d_producesSunMakeRules;
};

/**
 * Converts a command path (e.g. "/usr/bin/gcc-4.7") to a command name (e.g.
 * "gcc")
 */
std::string command_basename(const char *path);
inline std::string command_basename(std::string path)
{
    return command_basename(path.c_str());
}
} // namespace recc
} // namespace BloombergLP

#endif
