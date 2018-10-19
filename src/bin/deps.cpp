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

#include <cstring>
#include <iostream>
#include <regex>

using namespace BloombergLP::recc;
using namespace std;

const string HELP(
    "USAGE: deps<command>\n"
    "\n"
    "Attempts to determine the files needed to execute the given compiler\n"
    "command, then prints a newline-separated list of them.\n");

int main(int argc, char *argv[])
{
    parse_config_variables();
    string cwd = get_current_working_directory();

    if (argc <= 1 || strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "-h") == 0) {
        cerr << HELP;
        return 0;
    }
    try {
        auto deps =
            get_file_info(ParsedCommand(&argv[1], cwd.c_str())).dependencies;
        for (const auto &dep : deps) {
            cout << dep << endl;
        }
    }
    catch (const subprocess_failed_error &e) {
        exit(e.error_code);
    }
}
