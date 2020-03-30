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

namespace BloombergLP {
namespace recc {

CompilerDefaults::CompilerListType
CompilerDefaults::getCompilers(const CompilerFlavour &flavour)
{
    switch (flavour) {
        case CompilerFlavour::Gcc: {
            return {"gcc", "g++", "c++", "clang", "clang++"};
        }
        case CompilerFlavour::GccPreprocessor: {
            return {"gcc-preprocessor"};
        }
        case CompilerFlavour::SunCPP: {
            return {"CC"};
        }
        case CompilerFlavour::AIX: {
            return {"xlc", "xlc++", "xlC", "xlCcore", "xlc++core"};
        }
        case CompilerFlavour::SunC: {
            return {"cc", "c89", "c99"};
        }
        default:
            return {};
    }
}

} // namespace recc
} // namespace BloombergLP
