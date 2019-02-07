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

#ifndef INCLUDED_COMMANDLINEUTILS
#define INCLUDED_COMMANDLINEUTILS

#include <string>
#include <vector>

namespace BloombergLP {
namespace recc {

class CommandLineUtils {
  public:
    /* Utility function to test if a string starts with a prefix */
    static bool starts_with(const std::string &input,
                            const std::string &prefix);

    /**
     * Given an option appended to its arg like -I/foo/bar, splits it into
     * separate strings -I and /foo/bar.
     *
     * The list of options to split is hardcoded in the implementation file.
     * */
    static std::vector<std::string>
    split_option_from_arg(const std::string &optionArg);

    /**
     * Prepend all absolute paths in the command tokens with the pathPrefix
     */
    static std::vector<std::string> prepend_absolute_paths_in_compile_command(
        std::vector<std::string> commandArgs, const std::string &pathPrefix);
};

} // namespace recc
} // namespace BloombergLP

#endif
