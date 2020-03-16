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

#include <buildboxcommon_fileutils.h>
#include <fileutils.h>
#include <logging.h>

#include <cctype>
#include <compilerdefaults.h>
#include <cstring>
#include <fileutils.h>
#include <functional>
#include <logging.h>
#include <map>
#include <parsedcommand.h>
#include <utility>

namespace BloombergLP {
namespace recc {

/**
 * A command parser is a function that takes the following arguments:
 *
 * std::vector<string> *command
 * const char *workingDirectory
 * std::vector<string> *depsCommand
 * std::set<string> *outputs
 * bool *producesSunMakeRules
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

template <typename T> void UnusedVar(const T &) {}

#define COMMAND_PARSER_LAMBDA                                                 \
    [&](std::vector<std::string> * command, const char *workingDirectory,     \
        std::vector<std::string> *depsCommand,                                \
        std::set<std::string> *outputs, bool *producesSunMakeRules) -> bool

// Helper macros for writing command parsers
#define IF_GCC_OPTION_ARGUMENT(option, action)                                \
    if ((*command)[i] == option) {                                            \
        ++i;                                                                  \
        if (i < command->size()) {                                            \
            auto argument = (*command)[i];                                    \
            const int argumentLength = 2;                                     \
            UnusedVar(argumentLength);                                        \
            action                                                            \
        }                                                                     \
    }                                                                         \
    else if ((*command)[i].compare(0, strlen(option), option) == 0) {         \
        auto argument = (*command)[i].substr(strlen(option));                 \
        const int argumentLength = 1;                                         \
        UnusedVar(argumentLength);                                            \
        action                                                                \
    }
#define IF_EQUALS_OPTION_ARGUMENT(option, action)                             \
    if ((*command)[i] == option) {                                            \
        std::string argument;                                                 \
        const int argumentLength = 1;                                         \
        UnusedVar(argumentLength);                                            \
        action                                                                \
    }                                                                         \
    else if ((*command)[i].compare(0, strlen(option) + 1, option "=") == 0) { \
        auto argument = (*command)[i].substr(strlen(option) + 1);             \
        const int argumentLength = 1;                                         \
        UnusedVar(argumentLength);                                            \
        action                                                                \
    }
#define REMOVE_OPTION_FROM_COMMAND                                            \
    {                                                                         \
        command->erase(command->begin() + i - argumentLength + 1,             \
                       command->begin() + i + 1);                             \
        i -= argumentLength;                                                  \
    }
// First, make path relative to WORKING_DIR, then include in depsCommand.
// Then, check if original path is in PREFIX_MAP, replace and make relative.
#define INCLUDE_OPTION_IN_DEPS_COMMAND                                        \
    {                                                                         \
        auto original_arg = argument;                                         \
        ARGUMENT_IS_PATH                                                      \
        for (size_t j = i - argumentLength + 1; j < i + 1; j++) {             \
            depsCommand->push_back((*command)[j]);                            \
        }                                                                     \
        ARGUMENT_IS_SEARCH_PATH(original_arg);                                \
        ARGUMENT_IS_PATH;                                                     \
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
        REPLACE_ARGUMENT(                                                     \
            FileUtils::makePathRelative(argument, workingDirectory));         \
    }
#define ARGUMENT_IS_SEARCH_PATH(arg)                                          \
    if (arg.length() > 0) {                                                   \
        REPLACE_ARGUMENT(FileUtils::resolvePathFromPrefixMap(arg));           \
    }
#define OPTIONS_START()                                                       \
    bool isCompileCommand = false;                                            \
    UnusedVar(isCompileCommand);                                              \
    for (size_t i = 0; i < command->size(); ++i) {                            \
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
    else IF_GCC_OPTION_ARGUMENT(option, INCLUDE_OPTION_IN_DEPS_COMMAND)
#define EQUALS_OPTION_IS_INPUT_PATH(option)                                   \
    else IF_EQUALS_OPTION_ARGUMENT(option, INCLUDE_OPTION_IN_DEPS_COMMAND)
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
        std::string relative_command = (*command)[i];                         \
        if ((*command)[i].length() > 0 && (*command)[i][0] == '/') {          \
            relative_command =                                                \
                FileUtils::makePathRelative((*command)[i], workingDirectory); \
        }                                                                     \
        depsCommand->push_back(relative_command);                             \
        (*command)[i] = FileUtils::makePathRelative(                          \
            FileUtils::resolvePathFromPrefixMap((*command)[i]),               \
            workingDirectory);                                                \
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

// clang-format off
// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
// Suppress unused parameter warnings and sign conversion warnings stemming from the helper macros above.
_Pragma("GCC diagnostic push")
// Ignore sign conversion warnings resulting from checking string values in COMMAND_PARSER_LAMBDA
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
// Ignore unused parameter warnings resulting from compiler not recognizing usage of variables in COMMAND_PARSER_LAMBDA.
_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")

std::map<std::string, command_parser> make_command_parser_map()
{
    std::map<std::string, command_parser> result;
    std::vector<std::pair<std::set<std::string>, command_parser>> initList =
    {
    {CompilerDefaults::getCompilers(CompilerFlavour::Gcc), COMMAND_PARSER_LAMBDA {
        std::vector<std::string> preproOptions;

        OPTIONS_START()
        OPTION_INTERFERES_WITH_DEPS("-M")
        OPTION_INTERFERES_WITH_DEPS("-MD")
        OPTION_INTERFERES_WITH_DEPS("-MMD")
        OPTION_INTERFERES_WITH_DEPS("-MM")
        OPTION_INTERFERES_WITH_DEPS("-MG")
        OPTION_INTERFERES_WITH_DEPS("-MP")
        OPTION_INTERFERES_WITH_DEPS("-MV")
        GCC_OPTION_REDIRECTS_OUTPUT("-o")
        GCC_OPTION_REDIRECTS_OUTPUT("-MF")
        GCC_OPTION_REDIRECTS_OUTPUT("-MT")
        GCC_OPTION_REDIRECTS_OUTPUT("-MQ")
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
            result.at("gcc-preprocessor")(&preproOptions,
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
    {CompilerDefaults::getCompilers(CompilerFlavour::GccPreprocessor), COMMAND_PARSER_LAMBDA {
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
    {CompilerDefaults::getCompilers(CompilerFlavour::SunCPP), COMMAND_PARSER_LAMBDA {
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
    {CompilerDefaults::getCompilers(CompilerFlavour::AIX), COMMAND_PARSER_LAMBDA {
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
        // The file name is resolved, and pushed on at runtime time.
        *producesSunMakeRules = true;
        return isCompileCommand;
    }},
    {CompilerDefaults::getCompilers(CompilerFlavour::SunC), COMMAND_PARSER_LAMBDA {
#ifdef RECC_PLATFORM_COMPILER
        std::string compiler = ParsedCommand::command_basename(RECC_PLATFORM_COMPILER);
        if (result.count(compiler) > 0) {
            return result.at(compiler)(command,
                                               workingDirectory,
                                               depsCommand,
                                               outputs,
                                               producesSunMakeRules);
        }
#endif
        return false;
    }}
    }; // end initlist definition

    for (const auto &initPair : initList) {
        for (const auto &compiler : initPair.first) {
            result[compiler] = initPair.second;
        }
    }

    return result;

} // end make_command_parser_map

_Pragma("GCC diagnostic pop")

    // clang-format on

    ParsedCommand::ParsedCommand(std::vector<std::string> command,
                                 const char *workingDirectory)
    : d_compilerCommand(false), d_isClang(false),
      d_producesSunMakeRules(false), d_dependencyFileAIX(nullptr)
{
    if (command.size() > 0) {
        d_compiler = command_basename(command[0]);
        const auto commandParsers = make_command_parser_map();

        if (commandParsers.count(d_compiler) > 0) {
            d_compilerCommand = commandParsers.at(d_compiler)(
                &command, workingDirectory, &d_dependenciesCommand,
                &d_commandProducts, &d_producesSunMakeRules);
        }

        if (d_compiler == "clang" || d_compiler == "clang++") {
            d_isClang = true;

            if (RECC_DEPS_GLOBAL_PATHS) {
                // Clang mentions where it found crtbegin.o in
                // stderr with this flag.
                d_dependenciesCommand.push_back("-v");
            }
        }

        if (!d_isClang && CompilerDefaults::getCompilers(CompilerFlavour::AIX)
                                  .count(d_compiler) != 0) {
            // Create a temporary file, which will be used to write
            // dependency information to, on AIX.
            // The lifetime of the temporary file is the same as the instance
            // of parsedcommand.
            d_dependencyFileAIX =
                std::make_unique<buildboxcommon::TemporaryFile>(
                    buildboxcommon::TemporaryFile());
            // Push back created file name to vector of the dependency command.
            d_dependenciesCommand.push_back(d_dependencyFileAIX->strname());
        }
    }

    this->d_command = command;
}

std::string ParsedCommand::command_basename(const std::string &path)
{
    const auto lastSlash = path.rfind('/');
    const auto basename =
        (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    auto length = basename.length();

    // We get rid of "_r" suffixes in, for example, "./xlc++_r":
    const std::string rSuffix = "_r";
    if (length > 2 && basename.substr(length - rSuffix.length()) == rSuffix) {
        length -= 2;
    }
    else if (length > 3 &&
             basename.substr(length - 3, rSuffix.length()) == rSuffix) {
        length -= 3;
    }

    const auto is_version_character = [](const char character) {
        return (isdigit(character)) || character == '.' || character == '-';
    };

    while (length > 0 && is_version_character(basename[length - 1])) {
        --length;
    }

    return basename.substr(0, length);
}

std::vector<std::string>
ParsedCommand::vector_from_argv(const char *const *argv)
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

} // namespace recc
} // namespace BloombergLP
