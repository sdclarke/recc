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

#include <parsedcommandfactory.h>

#include <compilerdefaults.h>
#include <fileutils.h>

#include <buildboxcommon_exception.h>
#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>

namespace BloombergLP {
namespace recc {

/*
 * The maps below are used to construct the map used to find the coresponding
 * options depending on the compiler, returned from:
 * ParsedCommandModifiers::getParsedCommandMap()
 */
static const ParsedCommandFactory::CompilerOptionToFuncMapType GccRules = {
    // Interferes with dependencies
    {"-MD", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-MMD", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-M", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-MM", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-MG", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-MP", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-MV", ParsedCommandModifiers::parseInterfersWithDepsOption},
    // Compile options
    {"-c", ParsedCommandModifiers::parseIsCompileOption},
    // Redirects output
    {"-o", ParsedCommandModifiers::parseOptionRedirectsOutput},
    {"-MF", ParsedCommandModifiers::parseOptionRedirectsOutput},
    {"-MT", ParsedCommandModifiers::parseOptionRedirectsOutput},
    {"-MQ", ParsedCommandModifiers::parseOptionRedirectsOutput},
    // Input paths
    {"-include", ParsedCommandModifiers::parseIsInputPathOption},
    {"-imacros", ParsedCommandModifiers::parseIsInputPathOption},
    {"-I", ParsedCommandModifiers::parseIsInputPathOption},
    {"-iquote", ParsedCommandModifiers::parseIsInputPathOption},
    {"-isystem", ParsedCommandModifiers::parseIsInputPathOption},
    {"-idirafter", ParsedCommandModifiers::parseIsInputPathOption},
    {"-iprefix", ParsedCommandModifiers::parseIsInputPathOption},
    {"-isysroot", ParsedCommandModifiers::parseIsInputPathOption},
    {"--sysroot", ParsedCommandModifiers::parseIsEqualInputPathOption},
    // Preprocessor arguments
    {"-Wp,", ParsedCommandModifiers::parseIsPreprocessorArgOption},
    {"-Xpreprocessor", ParsedCommandModifiers::parseIsPreprocessorArgOption},
};

static const ParsedCommandFactory::CompilerOptionToFuncMapType
    GccPreprocessorRules = {
        // Interferes with dependencies
        {"-M", ParsedCommandModifiers::parseInterfersWithDepsOption},
        {"-MM", ParsedCommandModifiers::parseInterfersWithDepsOption},
        {"-MG", ParsedCommandModifiers::parseInterfersWithDepsOption},
        {"-MP", ParsedCommandModifiers::parseInterfersWithDepsOption},
        {"-MV", ParsedCommandModifiers::parseInterfersWithDepsOption},
        // Redirects output
        {"-o", ParsedCommandModifiers::parseOptionRedirectsOutput},
        {"-MF", ParsedCommandModifiers::parseOptionRedirectsOutput},
        {"-MT", ParsedCommandModifiers::parseOptionRedirectsOutput},
        {"-MQ", ParsedCommandModifiers::parseOptionRedirectsOutput},
        {"-MD", ParsedCommandModifiers::parseOptionRedirectsOutput},
        {"-MMD", ParsedCommandModifiers::parseOptionRedirectsOutput},
        // Input paths
        {"-include", ParsedCommandModifiers::parseIsInputPathOption},
        {"-imacros", ParsedCommandModifiers::parseIsInputPathOption},
        {"-I", ParsedCommandModifiers::parseIsInputPathOption},
        {"-iquote", ParsedCommandModifiers::parseIsInputPathOption},
        {"-isystem", ParsedCommandModifiers::parseIsInputPathOption},
        {"-idirafter", ParsedCommandModifiers::parseIsInputPathOption},
        {"-iprefix", ParsedCommandModifiers::parseIsInputPathOption},
        {"-isysroot", ParsedCommandModifiers::parseIsInputPathOption},
        {"--sysroot", ParsedCommandModifiers::parseIsEqualInputPathOption},
};

static const ParsedCommandFactory::CompilerOptionToFuncMapType SunCPPRules = {
    // Interferes with dependencies
    {"-xM", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-xM1", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-xMD", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-xMMD", ParsedCommandModifiers::parseInterfersWithDepsOption},
    // Redirects output
    {"-o", ParsedCommandModifiers::parseOptionRedirectsOutput},
    {"-xMF", ParsedCommandModifiers::parseOptionRedirectsOutput},
    // Input paths
    {"-I", ParsedCommandModifiers::parseIsInputPathOption},
    {"-include", ParsedCommandModifiers::parseIsInputPathOption},
    // Compile options
    {"-c", ParsedCommandModifiers::parseIsCompileOption},
    // Options not supported
    {"-xpch", ParsedCommandModifiers::parseOptionIsUnsupported},
    {"-xprofile", ParsedCommandModifiers::parseOptionIsUnsupported},
    {"-###", ParsedCommandModifiers::parseOptionIsUnsupported},
};

static const ParsedCommandFactory::CompilerOptionToFuncMapType AixRules = {
    // Interferes with dependencies
    {"-qmakedep", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-qmakedep=gcc", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-M", ParsedCommandModifiers::parseInterfersWithDepsOption},
    {"-qsyntaxonly", ParsedCommandModifiers::parseInterfersWithDepsOption},
    // Redirects output
    {"-o", ParsedCommandModifiers::parseOptionRedirectsOutput},
    {"-MF", ParsedCommandModifiers::parseOptionRedirectsOutput},
    {"-qexpfile", ParsedCommandModifiers::parseOptionRedirectsOutput},
    // Input paths
    {"-qinclude", ParsedCommandModifiers::parseIsInputPathOption},
    {"-I", ParsedCommandModifiers::parseIsInputPathOption},
    {"-qcinc", ParsedCommandModifiers::parseIsInputPathOption},
    // Compile options
    {"-c", ParsedCommandModifiers::parseIsCompileOption},
    // Options not supported
    {"-#", ParsedCommandModifiers::parseOptionIsUnsupported},
    {"-qshowpdf", ParsedCommandModifiers::parseOptionIsUnsupported},
    {"-qdump_class_hierachy",
     ParsedCommandModifiers::parseOptionIsUnsupported},
};

ParsedCommand ParsedCommandFactory::createParsedCommand(
    const std::vector<std::string> &command,
    const std::string &workingDirectory)
{
    if (command.empty()) {
        return ParsedCommand();
    }

    // Pass the option to the ParsedCommand constructor which will do things
    // such as populate various bools depending on if the compiler is of a
    // certain type.
    ParsedCommand parsedCommand(command[0]);

    // Get the map that maps compilers to options maps.
    const auto &parsedCommandMap =
        ParsedCommandModifiers::getParsedCommandMap();

    // Create a list from the vector.
    parsedCommand.d_originalCommand.insert(
        parsedCommand.d_originalCommand.begin(), command.begin(),
        command.end());

    // Map containing options corresponding to compiler passed in.
    CompilerOptionToFuncMapType optionToUse;

    // Find the options map that corresponds to the compiler.
    for (const auto &val : parsedCommandMap) {
        auto it = val.first.find(parsedCommand.d_compiler);
        if (it != val.first.end()) {
            optionToUse = val.second;
            break;
        }
    }

    // Parse and construct the command, and deps command vector.
    parseCommand(&parsedCommand, optionToUse, workingDirectory);

    // If unsupported options, set compile command to false, and return the
    // constructed parsedCommand.
    if (parsedCommand.d_containsUnsupportedOptions) {
        parsedCommand.d_compilerCommand = false;
        return parsedCommand;
    }

    // Handle gccpreprocessor options which were populated during the original
    // parsing of the command.
    // These options require special flags, before each option.
    if (parsedCommand.d_preProcessorOptions.size() > 0) {
        ParsedCommand preprocessorCommand;
        // Set preprecessor command to that created from parsing original
        // command, so it can be parsed.
        preprocessorCommand.d_originalCommand.insert(
            preprocessorCommand.d_originalCommand.begin(),
            parsedCommand.d_preProcessorOptions.begin(),
            parsedCommand.d_preProcessorOptions.end());

        parseCommand(&preprocessorCommand, GccPreprocessorRules,
                     workingDirectory);

        for (const auto &preproArg : preprocessorCommand.d_command) {
            parsedCommand.d_command.push_back("-Xpreprocessor");
            parsedCommand.d_command.push_back(preproArg);
        }

        for (const auto &preproArg :
             preprocessorCommand.d_dependenciesCommand) {
            parsedCommand.d_dependenciesCommand.push_back("-Xpreprocessor");
            parsedCommand.d_dependenciesCommand.push_back(preproArg);
        }

        for (const auto &preproArg : preprocessorCommand.d_commandProducts) {
            parsedCommand.d_commandProducts.insert(preproArg);
        }
    }

    // Insert default deps options into newly constructed parsedCommand deps
    // vector.
    // This vector is populated by the ParsedCommand constructor depending on
    // the compiler specified in the command.
    parsedCommand.d_dependenciesCommand.insert(
        parsedCommand.d_dependenciesCommand.end(),
        parsedCommand.d_defaultDepsCommand.begin(),
        parsedCommand.d_defaultDepsCommand.end());

    // d_originalCommand gets modified during the parsing of the
    // command-> Reset it.
    parsedCommand.d_originalCommand.insert(
        parsedCommand.d_originalCommand.begin(), command.begin(),
        command.end());

    return parsedCommand;
}

ParsedCommand
ParsedCommandFactory::createParsedCommand(char **argv,
                                          const char *workingDirectory)
{
    return createParsedCommand(vectorFromArgv(argv), workingDirectory);
}

ParsedCommand ParsedCommandFactory::createParsedCommand(
    std::initializer_list<std::string> command)
{
    return createParsedCommand(std::vector<std::string>(command), "");
}

void ParsedCommandFactory::parseCommand(
    ParsedCommand *command, const CompilerOptionToFuncMapType &options,
    const std::string &workingDirectory)
{
    // Iterate through the options map, comparing the options in the
    // command to each option, if matching, applying the coresponding option
    // function.
    while (!command->d_originalCommand.empty()) {
        const auto &curr_val = command->d_originalCommand.front();

        const auto &optionModifier =
            ParsedCommandModifiers::matchCompilerOptions(curr_val, options);

        if (optionModifier.second) {
            optionModifier.second(command, workingDirectory,
                                  optionModifier.first);
        }
        else {
            const std::string replacedPath =
                ParsedCommandModifiers::modifyRemotePath(curr_val,
                                                         workingDirectory);
            command->d_command.push_back(replacedPath);
            command->d_dependenciesCommand.push_back(curr_val);
            command->d_originalCommand.pop_front();
        }
    } // end while
}

std::vector<std::string>
ParsedCommandFactory::vectorFromArgv(const char *const *argv)
{
    std::vector<std::string> result;
    int i = 0;

    std::ostringstream arg_string;
    arg_string << "Parsing command:" << std::endl;
    while (argv[i] != nullptr) {
        std::string argstr = std::string(argv[i]);
        ++i;

        arg_string << "argv[" << i << "] = " << argstr << std::endl;
        result.push_back(argstr);
    }
    BUILDBOX_LOG_DEBUG(arg_string.str());

    return result;
}

std::pair<std::string, std::function<void(ParsedCommand *, const std::string &,
                                          const std::string &)>>
ParsedCommandModifiers::matchCompilerOptions(
    const std::string &option,
    const ParsedCommandFactory::CompilerOptionToFuncMapType &options)
{
    auto tempOption = option;

    if (!tempOption.empty() && tempOption.front() == '-') {

        // Check for an equal sign, if any, return left side.
        tempOption = tempOption.substr(0, tempOption.find("="));

        // First try finding an exact match, removing and parsing until an
        // equal sign. Remove any spaces from the option.
        tempOption.erase(
            remove_if(tempOption.begin(), tempOption.end(), ::isspace),
            tempOption.end());

        if (options.count(tempOption) > 0) {
            return std::make_pair(tempOption, options.at(tempOption));
        }

        // Second, try a substring search, iterating through all the
        // options in the map.
        for (const auto &option_map_val : options) {
            const auto val = option.substr(0, option_map_val.first.length());
            if (val == option_map_val.first) {
                return std::make_pair(option_map_val.first,
                                      option_map_val.second);
            }
        }
    }

    return std::make_pair("", nullptr);
}

void ParsedCommandModifiers::parseInterfersWithDepsOption(
    ParsedCommand *command, const std::string &, const std::string &)
{
    // Only push back to command vector.
    command->d_command.push_back(command->d_originalCommand.front());
    command->d_originalCommand.pop_front();
}

void ParsedCommandModifiers::parseIsInputPathOption(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option)
{
    gccOptionModifier(command, workingDirectory, option);
}

void ParsedCommandModifiers::parseIsEqualInputPathOption(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option)
{
    gccOptionModifier(command, workingDirectory, option);
}

void ParsedCommandModifiers::parseIsCompileOption(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &)
{
    command->d_compilerCommand = true;
    // Push back option (e.g "-c")
    appendAndRemoveOption(command, workingDirectory, false, true);
}

void ParsedCommandModifiers::parseOptionRedirectsOutput(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option)
{
    gccOptionModifier(command, workingDirectory, option, false, true);
}

void ParsedCommandModifiers::parseIsPreprocessorArgOption(
    ParsedCommand *command, const std::string &, const std::string &option)
{
    auto val = command->d_originalCommand.front();
    if (option == "-Wp,") {
        // parse comma separated list of args, and store in
        // commands preprocessor vector.
        auto optionList = val.substr(option.size());
        parseStageOptionList(optionList, &command->d_preProcessorOptions);
    }
    else if (option == "-Xpreprocessor") {
        // push back next arg
        command->d_originalCommand.pop_front();
        command->d_preProcessorOptions.push_back(
            command->d_originalCommand.front());
    }

    command->d_originalCommand.pop_front();
}

void ParsedCommandModifiers::parseOptionIsUnsupported(ParsedCommand *command,
                                                      const std::string &,
                                                      const std::string &)
{
    command->d_containsUnsupportedOptions = true;

    // append the rest of the command and deps command vector.
    command->d_dependenciesCommand.insert(command->d_dependenciesCommand.end(),
                                          command->d_originalCommand.begin(),
                                          command->d_originalCommand.end());

    command->d_command.insert(command->d_command.end(),
                              command->d_originalCommand.begin(),
                              command->d_originalCommand.end());

    // clear the original command so parsing stops.
    command->d_originalCommand.clear();
}

void ParsedCommandModifiers::gccOptionModifier(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option, bool toDeps, bool isOutput)
{
    auto val = command->d_originalCommand.front();
    // Space between option and input path (-I /usr/bin/include)
    if (val == option) {
        ParsedCommandModifiers::appendAndRemoveOption(
            command, workingDirectory, false, toDeps);
        // Push back corresponding path, but not into deps command
        ParsedCommandModifiers::appendAndRemoveOption(
            command, workingDirectory, true, toDeps, isOutput);
    }
    // No space between option and path (-I/usr/bin/include)
    // Or if "=" sign between option and path. (-I=/usr/bin/include)
    else {
        const auto equalPos = val.find('=');
        auto optionPath = val.substr(option.size());
        auto modifiedOption = option;

        if (equalPos != std::string::npos) {
            modifiedOption += "=";
            optionPath = val.substr(equalPos + 1);
        }

        const std::string replacedPath =
            ParsedCommandModifiers::modifyRemotePath(optionPath,
                                                     workingDirectory);

        command->d_command.push_back(modifiedOption + replacedPath);

        if (isOutput) {
            command->d_commandProducts.insert(replacedPath);
        }
        else if (toDeps) {
            command->d_dependenciesCommand.push_back(modifiedOption +
                                                     optionPath);
        }

        command->d_originalCommand.pop_front();
    }
}

void ParsedCommandModifiers::appendAndRemoveOption(
    ParsedCommand *command, const std::string &workingDirectory, bool isPath,
    bool toDeps, bool isOutput)
{
    auto option = command->d_originalCommand.front();
    if (isPath) {

        const std::string replacedPath =
            ParsedCommandModifiers::modifyRemotePath(option, workingDirectory);

        // If pushing back to dependencies command, do not replace the
        // path since this will be run locally.
        if (toDeps) {
            command->d_dependenciesCommand.push_back(option);
        }
        command->d_command.push_back(replacedPath);

        if (isOutput) {
            command->d_commandProducts.insert(replacedPath);
        }
    }
    else {
        // Append option to both vectors.
        command->d_command.push_back(option);
        if (toDeps) {
            command->d_dependenciesCommand.push_back(option);
        }
    }

    // Remove from original_command
    command->d_originalCommand.pop_front();
}

std::string
ParsedCommandModifiers::modifyRemotePath(const std::string &path,
                                         const std::string &workingDirectory)
{
    const auto replacedPath = FileUtils::resolvePathFromPrefixMap(path);
    return FileUtils::makePathRelative(replacedPath, workingDirectory.c_str());
}

void ParsedCommandModifiers::parseStageOptionList(
    const std::string &option, std::vector<std::string> *result)
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

ParsedCommandFactory::CompilerOptionToParsingFunctionMap
ParsedCommandModifiers::getParsedCommandMap()
{
    return {
        {SupportedCompilers::Gcc, GccRules},
        {SupportedCompilers::GccPreprocessor, GccPreprocessorRules},
        {SupportedCompilers::SunCPP, SunCPPRules},
        {SupportedCompilers::AIX, AixRules},
    };
}

} // namespace recc
} // namespace BloombergLP
