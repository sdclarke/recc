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

#ifndef INCLUDED_ACTIONBUILDER
#define INCLUDED_ACTIONBUILDER

#include <deps.h>
#include <protos.h>

#include <memory>
#include <unordered_map>

namespace BloombergLP {
namespace recc {

class ActionBuilder {
  public:
    /**
     * Build an action from the given ParsedCommand and working directory.
     *
     * Returns a nullptr if an action could not be built due to invoking a
     * non-compile command or trying to output files in directory unrelated
     * to the current working directory.
     *
     * "filenames" and "blobs" are used to store parsed input and output files,
     * which will get uploaded to CAS by the caller.
     */
    static std::shared_ptr<proto::Action>
    BuildAction(const ParsedCommand &command, const std::string &cwd,
                std::unordered_map<proto::Digest, std::string> *filenames,
                std::unordered_map<proto::Digest, std::string> *blobs);
};

} // namespace recc
} // namespace BloombergLP

#endif
