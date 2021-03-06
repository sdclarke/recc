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

#include <deps.h>

#include <compilerdefaults.h>
#include <env.h>
#include <subprocess.h>

#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace BloombergLP {
namespace recc {

std::set<std::string> Deps::dependencies_from_make_rules(
    const std::string &rules, bool is_sun_format, bool include_global_paths)
{
    std::set<std::string> result;
    bool saw_colon_on_line = false;
    bool saw_backslash = false;
    bool ignoring_file = false;

    std::string current_filename;
    for (const char &character : rules) {
        if (saw_backslash) {
            saw_backslash = false;
            if (character != '\n' && !ignoring_file && saw_colon_on_line) {
                current_filename += character;
            }
        }
        else if (character == '\\') {
            saw_backslash = true;
        }
        else if (character == ':' && !saw_colon_on_line) {
            saw_colon_on_line = true;
        }
        else if (character == '\n') {
            saw_colon_on_line = false;
            ignoring_file = false;
            if (!current_filename.empty()) {
                result.insert(current_filename);
            }
            current_filename.clear();
        }
        else if (character == ' ') {
            if (is_sun_format) {
                if (!current_filename.empty() && !ignoring_file &&
                    saw_colon_on_line) {
                    current_filename += character;
                }
            }
            else {
                ignoring_file = false;
                if (!current_filename.empty()) {
                    result.insert(current_filename);
                }
                current_filename.clear();
            }
        }
        else if (character == '/' && current_filename.empty() &&
                 !include_global_paths) {
            ignoring_file = true;
        }
        else if (!ignoring_file && saw_colon_on_line) {
            current_filename += character;
        }
    }

    if (!current_filename.empty()) {
        result.insert(current_filename);
    }

    return result;
}

std::string Deps::crtbegin_from_clang_v(const std::string &str)
{
    // Look for lines of the form:
    // ^Selected GCC installation: <path>$
    // and
    // ^Selected multilib: <path>;.*$
    // Then return these two paths joined (in order) with crtbegin.o appended.
    //
    // Reference:
    // https://github.com/llvm-mirror/clang/blob/69f63a0cc21da9f587125760f10610146c8c47c3/lib/Driver/ToolChains/Gnu.cpp#L1747

    std::regex re(
        "Selected GCC installation: ([^\n]*).*Selected multilib: ([^;\n]*)",
        std::regex::extended);
    std::smatch m;
    if (!std::regex_search(str, m, re)) {
        BUILDBOX_LOG_DEBUG("Failed to locate crtbegin.o for clang");
        return "";
    }

    std::ostringstream s;
    s << m[1];
    if (m[2] != ".") {
        // Avoid redundant .'s in the path.
        s << "/" << m[2];
    }
    s << "/crtbegin.o";

    std::string crtbegin_file = s.str();
    BUILDBOX_LOG_DEBUG("Found crtbegin.o for clang: " << crtbegin_file);

    return crtbegin_file;
}

CommandFileInfo Deps::get_file_info(const ParsedCommand &parsedCommand)
{
    CommandFileInfo result;
    bool is_clang = parsedCommand.is_clang();
    const auto subprocessResult =
        Subprocess::execute(parsedCommand.get_dependencies_command(), true,
                            is_clang, RECC_DEPS_ENV);

    if (subprocessResult.d_exitCode != 0) {
        std::string errorMsg = "Failed to execute get dependencies command: ";
        for (const auto &token : parsedCommand.get_dependencies_command()) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << subprocessResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << subprocessResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << subprocessResult.d_stdErr);
        throw subprocess_failed_error(subprocessResult.d_exitCode);
    }

    std::string dependencies = subprocessResult.d_stdOut;

    // If AIX compiler, read dependency information from temporary file.

    if (parsedCommand.is_AIX()) {
        dependencies = buildboxcommon::FileUtils::getFileContents(
            parsedCommand.get_aix_dependency_file_name().c_str());
    }

    result.d_dependencies = dependencies_from_make_rules(
        dependencies, parsedCommand.produces_sun_make_rules(),
        RECC_DEPS_GLOBAL_PATHS);

    if (RECC_DEPS_GLOBAL_PATHS && is_clang) {
        // Clang tries to locate GCC installations by looking for crtbegin.o
        // and then adjusts its system include paths. We need to upload this
        // file as if it were an input.
        std::string crtbegin =
            crtbegin_from_clang_v(subprocessResult.d_stdErr);
        if (crtbegin != "") {
            result.d_dependencies.insert(crtbegin);
        }
    }

    std::set<std::string> products;
    if (parsedCommand.get_products().size() > 0) {
        products = parsedCommand.get_products();
    }
    else {
        products = guess_products(result.d_dependencies);
    }

    for (const auto &product : products) {
        result.d_possibleProducts.insert(
            buildboxcommon::FileUtils::normalizePath(product.c_str()));
    }

    return result;
}

std::set<std::string> Deps::guess_products(const std::set<std::string> &deps)
{
    static const std::set<std::string> defaultOutputLocations = {"a.out"};
    static const std::set<std::string> defaultOutputExtensions = {".o", ".gch",
                                                                  ".d"};

    auto result = defaultOutputLocations;
    for (const auto &dep : deps) {
        auto name = dep.substr(0, dep.rfind('.'));
        const auto slash = name.rfind('/');

        if (slash != std::string::npos) {
            name = name.substr(slash + 1);
        }

        for (const auto &suffix : defaultOutputExtensions) {
            result.insert(name + suffix);
            result.insert(dep + suffix);
        }
    }

    return result;
}

} // namespace recc
} // namespace BloombergLP
