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

#include <actionbuilder.h>
#include <digestgenerator.h>
#include <env.h>
#include <fileutils.h>
#include <logging.h>
#include <reccdefaults.h>
#include <reccmetrics/durationmetrictimer.h>
#include <reccmetrics/metricguard.h>

#define TIMER_NAME_COMPILER_DEPS "recc.compiler_deps"
#define TIMER_NAME_BUILD_MERKLE_TREE "recc.build_merkle_tree"

namespace BloombergLP {
namespace recc {

std::shared_ptr<proto::Action>
ActionBuilder::BuildAction(const ParsedCommand &command,
                           const std::string &cwd, digest_string_umap *blobs,
                           digest_string_umap *digest_to_filecontents)
{

    if (!command.is_compiler_command() && !RECC_FORCE_REMOTE) {
        RECC_LOG_VERBOSE("Not a compiler command, so running locally.");
        RECC_LOG_VERBOSE(
            "(use RECC_FORCE_REMOTE=1 to force remote execution)");
        return nullptr;
    }

    std::string commandWorkingDirectory;
    NestedDirectory nestedDirectory;

    std::set<std::string> products = RECC_OUTPUT_FILES_OVERRIDE;
    if (!RECC_DEPS_DIRECTORY_OVERRIDE.empty()) {
        RECC_LOG_VERBOSE("Building Merkle tree using directory override");
        nestedDirectory = make_nesteddirectory(
            RECC_DEPS_DIRECTORY_OVERRIDE.c_str(), digest_to_filecontents);
    }
    else {
        std::set<std::string> deps = RECC_DEPS_OVERRIDE;

        if (RECC_DEPS_OVERRIDE.empty() && !RECC_FORCE_REMOTE) {
            RECC_LOG_VERBOSE("Getting dependencies");

            CommandFileInfo fileInfo;

            try { // Timed block
                reccmetrics::MetricGuard<reccmetrics::DurationMetricTimer> mt(
                    TIMER_NAME_COMPILER_DEPS, RECC_ENABLE_METRICS);
                fileInfo = get_file_info(command);
            }
            catch (const subprocess_failed_error &e) {
                RECC_LOG_VERBOSE("Running locally, to display the error.");
                return nullptr;
            }

            deps = fileInfo.d_dependencies;

            if (RECC_OUTPUT_DIRECTORIES_OVERRIDE.empty() &&
                RECC_OUTPUT_FILES_OVERRIDE.empty()) {
                products = fileInfo.d_possibleProducts;
            }
        }

        { // Timed block
            reccmetrics::MetricGuard<reccmetrics::DurationMetricTimer> mt(
                TIMER_NAME_BUILD_MERKLE_TREE, RECC_ENABLE_METRICS);

            RECC_LOG_VERBOSE("Building Merkle tree");
            int parentsNeeded = 0;
            for (const auto &dep : deps) {
                parentsNeeded =
                    std::max(parentsNeeded,
                             FileUtils::parent_directory_levels(dep.c_str()));
            }
            for (const auto &product : products) {
                parentsNeeded = std::max(
                    parentsNeeded,
                    FileUtils::parent_directory_levels(product.c_str()));
            }
            commandWorkingDirectory =
                FileUtils::last_n_segments(cwd.c_str(), parentsNeeded);

            for (const auto &dep : deps) {
                // If the dependency is an absolute path, leave
                // the merklePath untouched
                std::string merklePath;
                if (dep[0] == '/') {
                    merklePath = dep;
                }
                else {
                    merklePath = commandWorkingDirectory + "/" + dep;
                }
                merklePath = FileUtils::normalize_path(merklePath.c_str());

                std::shared_ptr<ReccFile> file =
                    ReccFileFactory::createFile(dep.c_str());
                if (!file) {
                    RECC_LOG_VERBOSE("Encountered unsupported file \""
                                     << dep << "\", skipping...");
                }
                nestedDirectory.add(file, merklePath.c_str());
                // Store the digest to the file content.
                (*digest_to_filecontents)[file->getDigest()] =
                    file->getFileContents();
            }
        }
    }

    if (!commandWorkingDirectory.empty()) {
        nestedDirectory.addDirectory(commandWorkingDirectory.c_str());
    }

    for (const auto &product : products) {
        if (!product.empty() && product[0] == '/') {
            RECC_LOG_VERBOSE("Command produces file in a location unrelated "
                             "to the current directory, so running locally.");
            RECC_LOG_VERBOSE(
                "(use RECC_OUTPUT_[FILES|DIRECTORIES]_OVERRIDE to override)");
            return nullptr;
        }
    }

    const auto directoryDigest = nestedDirectory.to_digest(blobs);

    proto::Command commandProto;

    for (const auto &arg : command.get_command()) {
        commandProto.add_arguments(arg);
    }
    for (const auto &envIter : RECC_REMOTE_ENV) {
        auto envVar = commandProto.add_environment_variables();
        envVar->set_name(envIter.first);
        envVar->set_value(envIter.second);
    }
    for (const auto &product : products) {
        commandProto.add_output_files(product);
    }
    for (const auto &directory : RECC_OUTPUT_DIRECTORIES_OVERRIDE) {
        commandProto.add_output_directories(directory);
    }
    for (const auto &platformIter : RECC_REMOTE_PLATFORM) {
        auto property = commandProto.mutable_platform()->add_properties();
        property->set_name(platformIter.first);
        property->set_value(platformIter.second);
    }
    // If deps paths aren't absolute, they are made absolute by having the CWD
    // prepended, and then normalized and replaced. If that is the case, and
    // the CWD contains a replaced prefix, then replace it.
    *commandProto.mutable_working_directory() =
        FileUtils::resolve_path_from_prefix_map(commandWorkingDirectory);
    RECC_LOG_VERBOSE("Command: " << commandProto.ShortDebugString());
    const auto commandDigest = DigestGenerator::make_digest(commandProto);
    (*blobs)[commandDigest] = commandProto.SerializeAsString();

    proto::Action action;
    *action.mutable_command_digest() = commandDigest;
    *action.mutable_input_root_digest() = directoryDigest;
    action.set_do_not_cache(RECC_ACTION_UNCACHEABLE);
    return std::make_shared<proto::Action>(action);
}

} // namespace recc
} // namespace BloombergLP
