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

#include <compilerdefaults.h>
#include <env.h>
#include <parsedcommand.h>

namespace BloombergLP {
namespace recc {

ParsedCommand::ParsedCommand(const std::string &command)
    : d_compilerCommand(false), d_isClang(false),
      d_producesSunMakeRules(false), d_containsUnsupportedOptions(false),
      d_dependencyFileAIX(nullptr)
{
    if (command.empty()) {
        return;
    }

    d_compiler = commandBasename(command);
    if (SupportedCompilers::Gcc.count(d_compiler)) {
        d_defaultDepsCommand = SupportedCompilers::GccDefaultDeps;
        if (d_compiler == "clang" || d_compiler == "clang++") {
            d_isClang = true;
            if (RECC_DEPS_GLOBAL_PATHS) {
                // Clang mentions where it found crtbegin.o in
                // stderr with this flag.
                d_defaultDepsCommand.push_back("-v");
            }
        }
        else {
            d_isClang = false;
        }
        return;
    }

    if (SupportedCompilers::SunCPP.count(d_compiler)) {
        d_defaultDepsCommand = SupportedCompilers::SunCPPDefaultDeps;
        d_producesSunMakeRules = true;
        return;
    }

    if (SupportedCompilers::AIX.count(d_compiler)) {
        d_defaultDepsCommand = SupportedCompilers::AIXDefaultDeps;
        d_producesSunMakeRules = true;
        // Create a temporary file, which will be used to write
        // dependency information to, on AIX.
        // The lifetime of the temporary file is the same as the
        // instance of parsedcommand.
        d_dependencyFileAIX = std::make_unique<buildboxcommon::TemporaryFile>(
            buildboxcommon::TemporaryFile());
        // Push back created file name to vector of the dependency
        // command->
        d_defaultDepsCommand.push_back(d_dependencyFileAIX->strname());
        return;
    }
}

std::string ParsedCommand::commandBasename(const std::string &path)
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

} // namespace recc
} // namespace BloombergLP
