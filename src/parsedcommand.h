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
    while (argv[i] != nullptr) {
        result.push_back(std::string(argv[i]));
        ++i;
    }
    return result;
}

/**
 * Represents the result of parsing a compiler command.
 */
class ParsedCommand {
  public:
    ParsedCommand(std::vector<std::string> command);
    ParsedCommand(char **argv) : ParsedCommand(vector_from_argv(argv)) {}
    ParsedCommand(std::initializer_list<std::string> command)
        : ParsedCommand(std::vector<std::string>(command))
    {
    }

    /**
     * Returns true if the given command is a supported compiler command.
     */
    bool is_compiler_command() const { return compilerCommand; }

    /**
     * Returns the original command that was passed to the constructor.
     */
    std::vector<std::string> get_command() const { return command; }

    /**
     * Return a command that prints this command's dependencies in Makefile
     * format. If this command is not a supported compiler command, the result
     * is undefined.
     */
    std::vector<std::string> get_dependencies_command() const
    {
        return dependenciesCommand;
    }

    /**
     * Return the output files specified in the command arguments.
     *
     * This is not necessarily all of the files the command will create. (For
     * example, if no output files are specified, many compilers will write to
     * a.out by default.)
     */
    std::set<std::string> get_products() const { return commandProducts; }

    /**
     * If true, the dependencies command will produce nonstandard Sun-style
     * make rules where one dependency is listed per line and spaces aren't
     * escaped.
     */
    bool produces_sun_make_rules() const { return producesSunMakeRules; }

  private:
    bool compilerCommand;
    std::vector<std::string> command;
    std::vector<std::string> dependenciesCommand;
    std::set<std::string> commandProducts;
    bool producesSunMakeRules;
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
