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

#include <parsedcommand.h>

#include <fileutils.h>

#include <cstring>
#include <functional>
#include <map>
#include <utility>

namespace BloombergLP {
namespace recc {

/**
 * A command parser is a function that takes the following arguments:
 *
 * -std::vector<string> *command
 * - const char *workingDirectory
 * -std::vector<string> *depsCommand
 * -std::set<string> *outputs
 * - bool *producesSunMakeRules
 *
 * It stores a command to get the dependencies in depsCommand, stores
 * any outputs it can determine (e.g. by parsing "-o" flags) in outputs,
 * and returns a boolean indicating success.
 *
 * If a non-null working directory is passed in, the command will be
 * modified to replace absolute paths with relative ones.
 */
typedef std::function<bool(std::vector<std::string> *, const char *,
                           std::vector<std::string> *, std::set<std::string> *,
                           bool *)>
    command_parser;

#define COMMAND_PARSER_LAMBDA                                                 \
    [](std::vector<std::string> * command, const char *workingDirectory,      \
       std::vector<std::string> *depsCommand, std::set<std::string> *outputs, \
       bool *producesSunMakeRules) -> bool

// Helper macros for writing command parsers
#define IF_GCC_OPTION_ARGUMENT(option, action)                                \
    if ((*command)[i] == option) {                                            \
        ++i;                                                                  \
        if (i < command->size()) {                                            \
            auto argument = (*command)[i];                                    \
            const int argumentLength = 2;                                     \
            action                                                            \
        }                                                                     \
    }                                                                         \
    else if ((*command)[i].compare(0, strlen(option), option) == 0) {         \
        auto argument = (*command)[i].substr(strlen(option));                 \
        const int argumentLength = 1;                                         \
        action                                                                \
    }
#define IF_EQUALS_OPTION_ARGUMENT(option, action)                             \
    if ((*command)[i] == option) {                                            \
        std::string argument;                                                 \
        const int argumentLength = 1;                                         \
        action                                                                \
    }                                                                         \
    else if ((*command)[i].compare(0, strlen(option) + 1, option "=") == 0) { \
        auto argument = (*command)[i].substr(strlen(option) + 1);             \
        const int argumentLength = 1;                                         \
        action                                                                \
    }
#define REMOVE_OPTION_FROM_COMMAND                                            \
    {                                                                         \
        command->erase(command->begin() + i - argumentLength + 1,             \
                       command->begin() + i + 1);                             \
        i -= argumentLength;                                                  \
    }

#define INCLUDE_OPTION_IN_DEPS_COMMAND                                        \
    {                                                                         \
        for (int j = i - argumentLength + 1; j < i + 1; j++)                  \
            depsCommand->push_back((*command)[j]);                            \
    }
#define REPLACE_ARGUMENT(new_arg)                                             \
    {                                                                         \
        auto new_argument = new_arg;                                          \
        (*command)[i] =                                                       \
            (*command)[i].replace((*command)[i].length() - argument.length(), \
                                  argument.length(), new_argument);           \
        argument = new_argument;                                              \
    }
#define ARGUMENT_IS_PATH                                                      \
    if (argument.length() > 0 && argument[0] == '/') {                        \
        REPLACE_ARGUMENT(make_path_relative(argument, workingDirectory));     \
    }

#define OPTIONS_START()                                                       \
    bool isCompileCommand = false;                                            \
    for (int i = 0; i < command->size(); ++i) {                               \
        if (false) {                                                          \
        }
#define OPTION_INTERFERES_WITH_DEPS(option)                                   \
    else if ((*command)[i] == option) {}
#define GCC_OPTION_REDIRECTS_OUTPUT(option)                                   \
    else IF_GCC_OPTION_ARGUMENT(option, ARGUMENT_IS_PATH;                     \
                                outputs->insert(argument);)
#define EQUALS_OPTION_REDIRECTS_OUTPUT(option)                                \
    else IF_EQUALS_OPTION_ARGUMENT(                                           \
        option, ARGUMENT_IS_PATH;                                             \
        if (argument.length() > 0) { outputs->insert(argument); })
#define GCC_OPTION_IS_INPUT_PATH(option)                                      \
    else IF_GCC_OPTION_ARGUMENT(option, ARGUMENT_IS_PATH;                     \
                                INCLUDE_OPTION_IN_DEPS_COMMAND)
#define EQUALS_OPTION_IS_INPUT_PATH(option)                                   \
    else IF_EQUALS_OPTION_ARGUMENT(option, ARGUMENT_IS_PATH;                  \
                                   INCLUDE_OPTION_IN_DEPS_COMMAND)
#define OPTION_INDICATES_COMPILE(option)                                      \
    else if ((*command)[i] == option)                                         \
    {                                                                         \
        depsCommand->push_back((*command)[i]);                                \
        isCompileCommand = true;                                              \
    }
#define OPTION_UNSUPPORTED(option)                                            \
    else if ((*command)[i] == option) { return false; }
#define GCC_OPTION_UNSUPPORTED(option)                                        \
    else IF_GCC_OPTION_ARGUMENT(option, return false;)
#define EQUALS_OPTION_UNSUPPORTED(option)                                     \
    else IF_EQUALS_OPTION_ARGUMENT(option, return false;)
#define OPTIONS_END()                                                         \
    else                                                                      \
    {                                                                         \
        if ((*command)[i].length() > 0 && (*command)[i][0] == '/') {          \
            (*command)[i] =                                                   \
                make_path_relative((*command)[i], workingDirectory);          \
        }                                                                     \
        depsCommand->push_back((*command)[i]);                                \
    }                                                                         \
    }

/**
 * Parse a comma-separated list and store the results in the given vector.
 */
void parse_stage_option_list(const std::string &option,
                             std::vector<std::string> *result)
{

    bool quoted = false;
    std::string current;
    for (const char &character : option) {
        if (character == '\'') {
            quoted = !quoted;
        }
        else if (character == ',' && !quoted) {
            result->push_back(current);
            current = std::string();
        }
        else {
            current += character;
        }
    }
    result->push_back(current);
}

std::map<std::string, command_parser> make_command_parser_map(
    std::initializer_list<std::pair<std::set<std::string>, command_parser>>
        initList)
{
    std::map<std::string, command_parser> result;
    for (const auto &initPair : initList) {
        for (const auto &compiler : initPair.first) {
            result[compiler] = initPair.second;
        }
    }
    return result;
}

// clang-format off
const std::map<std::string, command_parser> commandParsers = make_command_parser_map({
    {{"gcc", "g++", "c++"}, COMMAND_PARSER_LAMBDA {
            std::vector<std::string> preproOptions;

        OPTIONS_START()
        OPTION_INTERFERES_WITH_DEPS("-M")
        OPTION_INTERFERES_WITH_DEPS("-MM")
        OPTION_INTERFERES_WITH_DEPS("-MG")
        OPTION_INTERFERES_WITH_DEPS("-MP")
        OPTION_INTERFERES_WITH_DEPS("-MV")
        GCC_OPTION_REDIRECTS_OUTPUT("-o")
        GCC_OPTION_REDIRECTS_OUTPUT("-MF")
        GCC_OPTION_REDIRECTS_OUTPUT("-MT")
        GCC_OPTION_REDIRECTS_OUTPUT("-MQ")
        GCC_OPTION_REDIRECTS_OUTPUT("-MD")
        GCC_OPTION_REDIRECTS_OUTPUT("-MMD")
        GCC_OPTION_IS_INPUT_PATH("-include")
        GCC_OPTION_IS_INPUT_PATH("-imacros")
        GCC_OPTION_IS_INPUT_PATH("-I")
        GCC_OPTION_IS_INPUT_PATH("-iquote")
        GCC_OPTION_IS_INPUT_PATH("-isystem")
        GCC_OPTION_IS_INPUT_PATH("-idirafter")
        GCC_OPTION_IS_INPUT_PATH("-iprefix")
        GCC_OPTION_IS_INPUT_PATH("-isysroot")
        EQUALS_OPTION_IS_INPUT_PATH("--sysroot")
        OPTION_INDICATES_COMPILE("-c")
        else IF_GCC_OPTION_ARGUMENT("-Wp,",
            REMOVE_OPTION_FROM_COMMAND;
            parse_stage_option_list(argument, &preproOptions);
        )
        else IF_GCC_OPTION_ARGUMENT("-Xpreprocessor",
            REMOVE_OPTION_FROM_COMMAND;
            preproOptions.push_back(argument);
        )

        OPTIONS_END()

        if (preproOptions.size() > 0) {
            std::vector<std::string> preproDepsCommand;
            commandParsers.at("gcc-preprocessor")(&preproOptions,
                                                  workingDirectory,
                                                  &preproDepsCommand,
                                                  outputs,
                                                  producesSunMakeRules);
            for (const auto& preproArg : preproOptions) {
                command->push_back("-Xpreprocessor");
                command->push_back(preproArg);
            }
            for (const auto& preproArg : preproDepsCommand) {
                depsCommand->push_back("-Xpreprocessor");
                depsCommand->push_back(preproArg);
            }
        }

        depsCommand->push_back("-M");
        return isCompileCommand;
    }},
    {{"gcc-preprocessor"}, COMMAND_PARSER_LAMBDA {
        OPTIONS_START()
        OPTION_INTERFERES_WITH_DEPS("-M")
        OPTION_INTERFERES_WITH_DEPS("-MM")
        OPTION_INTERFERES_WITH_DEPS("-MG")
        OPTION_INTERFERES_WITH_DEPS("-MP")
        OPTION_INTERFERES_WITH_DEPS("-MV")
        GCC_OPTION_REDIRECTS_OUTPUT("-o")
        GCC_OPTION_REDIRECTS_OUTPUT("-MF")
        GCC_OPTION_REDIRECTS_OUTPUT("-MT")
        GCC_OPTION_REDIRECTS_OUTPUT("-MQ")
        GCC_OPTION_REDIRECTS_OUTPUT("-MD")
        GCC_OPTION_REDIRECTS_OUTPUT("-MMD")
        GCC_OPTION_IS_INPUT_PATH("-include")
        GCC_OPTION_IS_INPUT_PATH("-imacros")
        GCC_OPTION_IS_INPUT_PATH("-I")
        GCC_OPTION_IS_INPUT_PATH("-iquote")
        GCC_OPTION_IS_INPUT_PATH("-isystem")
        GCC_OPTION_IS_INPUT_PATH("-idirafter")
        GCC_OPTION_IS_INPUT_PATH("-iprefix")
        GCC_OPTION_IS_INPUT_PATH("-isysroot")
        EQUALS_OPTION_IS_INPUT_PATH("--sysroot")
        OPTIONS_END()

        return false;
    }},
    {{"CC"}, COMMAND_PARSER_LAMBDA {
        OPTIONS_START()
        OPTION_INTERFERES_WITH_DEPS("-xM")
        OPTION_INTERFERES_WITH_DEPS("-xM1")
        OPTION_INTERFERES_WITH_DEPS("-xMD")
        OPTION_INTERFERES_WITH_DEPS("-xMMD")
        GCC_OPTION_REDIRECTS_OUTPUT("-o")
        GCC_OPTION_REDIRECTS_OUTPUT("-xMF")
        GCC_OPTION_IS_INPUT_PATH("-I")
        GCC_OPTION_IS_INPUT_PATH("-include")
        OPTION_INDICATES_COMPILE("-c")
        EQUALS_OPTION_UNSUPPORTED("-xpch")
        EQUALS_OPTION_UNSUPPORTED("-xprofile")
        OPTION_UNSUPPORTED("-###")
        OPTIONS_END()
        depsCommand->push_back("-xM");
        *producesSunMakeRules = true;
        return isCompileCommand;
    }},
    {{"xlc", "xlc++", "xlC", "xlCcore", "xlc++core"}, COMMAND_PARSER_LAMBDA {
        OPTIONS_START()
        OPTION_INTERFERES_WITH_DEPS("-qmakedep")
        OPTION_INTERFERES_WITH_DEPS("-qmakedep=gcc")
        OPTION_INTERFERES_WITH_DEPS("-M")
        OPTION_INTERFERES_WITH_DEPS("-qsyntaxonly")
        GCC_OPTION_REDIRECTS_OUTPUT("-MF")
        GCC_OPTION_REDIRECTS_OUTPUT("-o")
        EQUALS_OPTION_REDIRECTS_OUTPUT("-qexpfile")
        EQUALS_OPTION_IS_INPUT_PATH("-qcinc")
        GCC_OPTION_IS_INPUT_PATH("-I")
        EQUALS_OPTION_IS_INPUT_PATH("-qinclude")
        OPTION_INDICATES_COMPILE("-c")
        OPTION_UNSUPPORTED("-#")
        OPTION_UNSUPPORTED("-qshowpdf")
        OPTION_UNSUPPORTED("-qdump_class_hierarchy")
        OPTIONS_END()
        depsCommand->push_back("-qsyntaxonly");
        depsCommand->push_back("-M");
        depsCommand->push_back("-MF");
        depsCommand->push_back("/dev/stdout");
        *producesSunMakeRules = true;
        return isCompileCommand;
    }},
    {{"cc", "c89", "c99"}, COMMAND_PARSER_LAMBDA {
#ifdef RECC_PLATFORM_COMPILER
        std::string compiler = command_basename(RECC_PLATFORM_COMPILER);
        if (commandParsers.count(compiler) > 0) {
            return commandParsers.at(compiler)(command,
                                               workingDirectory,
                                               depsCommand,
                                               outputs,
                                               producesSunMakeRules);
        }
#endif
        return false;
    }}
});
// clang-format on

ParsedCommand::ParsedCommand(std::vector<std::string> command,
                             const char *workingDirectory)
{
    d_compilerCommand = false;
    d_producesSunMakeRules = false;
    if (command.size() > 0) {
        auto basename = command_basename(command[0]);
        if (commandParsers.count(basename) > 0) {
            d_compilerCommand = commandParsers.at(basename)(
                &command, workingDirectory, &d_dependenciesCommand,
                &d_commandProducts, &d_producesSunMakeRules);
        }
    }
    this->d_command = command;
}

bool is_version_character(char character)
{
    return (character >= '0' && character <= '9') || character == '.' ||
           character == '-';
}

std::string command_basename(const char *path)
{
    const char *lastSlash = strrchr(path, '/');
    const char *basename = lastSlash == nullptr ? path : lastSlash + 1;
    int length = strlen(basename);
    if (basename[length - 2] == '_' && basename[length - 1] == 'r') {
        length -= 2;
    }
    else if (basename[length - 3] == '_' && basename[length - 2] == 'r') {
        length -= 3;
    }
    while (is_version_character(basename[length - 1]) && length > 0) {
        --length;
    }
    return std::string(basename, length);
}
}
}
