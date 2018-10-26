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

#include <env.h>
#include <fileutils.h>
#include <subprocess.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

using namespace std;

namespace BloombergLP {
namespace recc {

set<string> dependencies_from_make_rules(string rules, bool is_sun_format,
                                         bool include_global_paths)
{
    set<string> result;
    bool saw_colon_on_line = false;
    bool saw_backslash = false;
    bool ignoring_file = false;
    string current_filename;
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

CommandFileInfo get_file_info(ParsedCommand parsedCommand)
{
    CommandFileInfo result;
    auto subprocessResult = execute(parsedCommand.get_dependencies_command(),
                                    true, false, RECC_DEPS_ENV);
    if (subprocessResult.exitCode != 0) {
        throw subprocess_failed_error(subprocessResult.exitCode);
    }
    result.dependencies = dependencies_from_make_rules(
        subprocessResult.stdOut, parsedCommand.produces_sun_make_rules(),
        false);
    set<string> products;
    if (parsedCommand.get_products().size() > 0) {
        products = parsedCommand.get_products();
    }
    else {
        products = guess_products(result.dependencies);
    }

    for (const auto &product : products) {
        result.possibleProducts.insert(normalize_path(product.c_str()));
    }
    return result;
}

const set<string> DEFAULT_OUTPUT_LOCATIONS = {"a.out"};
const set<string> DEFAULT_OUTPUT_EXTENSIONS = {".o", ".gch", ".d"};

set<string> guess_products(set<string> deps)
{
    auto result = DEFAULT_OUTPUT_LOCATIONS;
    for (const auto &dep : deps) {
        auto name = dep.substr(0, dep.rfind('.'));
        auto slash = name.rfind('/');
        if (slash != string::npos) {
            name = name.substr(slash + 1);
        }
        for (const auto &suffix : DEFAULT_OUTPUT_EXTENSIONS) {
            result.insert(name + suffix);
            result.insert(dep + suffix);
        }
    }
    return result;
}
} // namespace recc
} // namespace BloombergLP
