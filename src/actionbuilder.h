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
#include <merklize.h>
#include <protos.h>

#include <memory>
#include <unordered_map>

namespace BloombergLP {
namespace recc {

extern std::mutex ContainerWriteMutex;
extern std::mutex LogWriteMutex;

struct ActionBuilder {
    /**
     * Build an `Action` from the given `ParsedCommand` and working directory.
     *
     * Returns `nullptr` if an action could not be built due to invoking a
     * non-compile command, an output files in specified in a directory
     * unrelated to the current working directory, or a command that does not
     * contain either a relative or absolute path to an executable.
     *
     * `digest_to_filecontents` and `blobs` are used to store parsed input and
     * output files, which will get uploaded to CAS by the caller.
     */
    static std::shared_ptr<proto::Action>
    BuildAction(const ParsedCommand &command, const std::string &cwd,
                digest_string_umap *digest_to_filecontents,
                digest_string_umap *blobs);

  protected: // for unit testing
    static proto::Command generateCommandProto(
        const std::vector<std::string> &command,
        const std::set<std::string> &products,
        const std::set<std::string> &outputDirectories,
        const std::map<std::string, std::string> &remoteEnvironment,
        const std::map<std::string, std::string> &platformProperties,
        const std::string &workingDirectory);

    /**
     * Populates a `Command` protobuf from a `ParsedCommand` and additional
     * information.
     *
     * It sets the command's arguments, its output directories, the environment
     * variables and platform properties for the remote.
     */
    static proto::Command populateCommandProto(
        const std::vector<std::string> &command,
        const std::set<std::string> &products,
        const std::set<std::string> &outputDirectories,
        const std::map<std::string, std::string> &remoteEnvironment,
        const std::map<std::string, std::string> &platformProperties,
        const std::string &workingDirectory);

    /**
     * Given a list of paths to dependency and output files, builds a
     * Merkle tree.
     *
     * Adds the files to `NestedDirectory` and `digest_to_filecontents`.
     *
     * If necessary, modifies the contents of `commandWorkingDirectory`.
     */
    static void buildMerkleTree(const std::set<std::string> &dependencies,
                                const std::string &cwd,
                                NestedDirectory *nestedDirectory,
                                digest_string_umap *digest_to_filecontents);

    /**
     * Gathers the `CommandFileInfo` belonging to the given `command` and
     * populates its dependency and product list (the latter only if no
     * overrides are set).
     */
    static void getDependencies(const ParsedCommand &command,
                                std::set<std::string> *dependencies,
                                std::set<std::string> *products);

    /** Scans the list of dependencies and output files and strips
     * `workingDirectory` to the level of the common ancestor.
     */
    static std::string
    commonAncestorPath(const std::set<std::string> &dependencies,
                       const std::set<std::string> &products,
                       const std::string &workingDirectory);

    /**
     * If prefix is not empty, prepends it to the working directory path.
     * Otherwise `workingDirectory` is return unmodified.
     */
    static std::string
    prefixWorkingDirectory(const std::string &workingDirectory,
                           const std::string &prefix);
};

} // namespace recc
} // namespace BloombergLP

#endif
