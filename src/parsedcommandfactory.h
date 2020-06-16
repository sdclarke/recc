// Copyright 2020 Bloomberg Finance L.P
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

#ifndef INCLUDED_PARSEDCOMMANDFACTORY
#define INCLUDED_PARSEDCOMMANDFACTORY

#include <compilerdefaults.h>
#include <functional>
#include <initializer_list>
#include <list>
#include <logging.h>
#include <memory>
#include <parsedcommand.h>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BloombergLP {
namespace recc {

class ParsedCommandFactory {
  public:
    /**
     * An unordered_map from a string to a function pointer.
     */
    typedef std::unordered_map<
        std::string, std::function<void(ParsedCommand *, const std::string &,
                                        const std::string &)>>
        CompilerOptionToFuncMapType;
    /**
     * An unordered_map from a set of strings to a CompilerOptionToFuncMapType.
     */
    typedef std::unordered_map<SupportedCompilers::CompilerListType,
                               CompilerOptionToFuncMapType,
                               CompilerListTypeHasher>
        CompilerOptionToParsingFunctionMap;

    /**
     * Default overloaded factory methods for creating a parsedCommand.
     * Calling these methods is the only way to create a parsedCommand.
     */
    static ParsedCommand
    createParsedCommand(const std::vector<std::string> &command,
                        const std::string &workingDirectory);
    static ParsedCommand createParsedCommand(char **argv,
                                             const char *workingDirectory);
    static ParsedCommand
    createParsedCommand(std::initializer_list<std::string> command);

    /**
     * Convert a null-terminated list of C strings to a vector of C++
     * strings.
     */
    static std::vector<std::string> vectorFromArgv(const char *const *argv);

  private:
    /**
     *  This method iterates through the parsedCommandMap, comparing the
     * options in the command to each option in the map, if matching, applying
     * the coresponding option function.
     *
     * This method modifies the state of the passed in ParsedCommand object.
     */
    static void parseCommand(ParsedCommand *command,
                             const CompilerOptionToFuncMapType &options,
                             const std::string &workingDirectory);

    ParsedCommandFactory() = delete;
};

struct ParsedCommandModifiers {

    static void
    parseInterfersWithDepsOption(ParsedCommand *command,
                                 const std::string &workingDirectory,
                                 const std::string &option);

    static void parseIsInputPathOption(ParsedCommand *command,
                                       const std::string &workingDirectory,
                                       const std::string &option);

    static void
    parseIsEqualInputPathOption(ParsedCommand *command,
                                const std::string &workingDirectory,
                                const std::string &option);

    static void parseIsCompileOption(ParsedCommand *command,
                                     const std::string &workingDirectory,
                                     const std::string &option);

    static void parseOptionRedirectsOutput(ParsedCommand *command,
                                           const std::string &workingDirectory,
                                           const std::string &option);

    static void
    parseIsPreprocessorArgOption(ParsedCommand *command,
                                 const std::string &workingDirectory,
                                 const std::string &option);

    static void parseOptionIsUnsupported(ParsedCommand *command,
                                         const std::string &workingDirectory,
                                         const std::string &option);

    /**
     * This helper deals with gcc options parsing, which can have a space after
     * an option, or no space.
     **/
    static void gccOptionModifier(ParsedCommand *command,
                                  const std::string &workingDirectory,
                                  const std::string &option,
                                  bool toDeps = true, bool isOutput = false);
    /**
     * This helper deals with pushing the correct command to the various
     * vectors in parsedCommand, depending on things such as if the command
     * specifies output, or is needed by the dependency command run locally.
     **/
    static void appendAndRemoveOption(ParsedCommand *command,
                                      const std::string &workingDirectory,
                                      bool isPath, bool toDeps,
                                      bool isOutput = false);
    /**
     * This helper deals with modifing paths, by returning:
     * 1. Path made relative to the working directory.
     * 2. Path replaced if matching a option in RECC_PREFIX_MAP, and then made
     * relative to the working directory.
     * The first path can be run locally, the second is used for remoteing the
     * command.
     */
    static std::pair<std::string, std::string>
    modifyPaths(const std::string &path, const std::string &workingDirectory);

    /**
     * Parse a comma-separated list and store the results in the given
    vector.
     */
    static void parseStageOptionList(const std::string &option,
                                     std::vector<std::string> *result);

    /**
     * Constructs and returns a CompilerOptionToParsingFunctionMap.
     */
    static ParsedCommandFactory::CompilerOptionToParsingFunctionMap
    getParsedCommandMap();
};

} // namespace recc
} // namespace BloombergLP

#endif
