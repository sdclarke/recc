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

#include <actionbuilder.h>

#include <digestgenerator.h>
#include <env.h>
#include <fileutils.h>
#include <reccdefaults.h>
#include <threadutils.h>

#include <buildboxcommon_logging.h>
#include <buildboxcommonmetrics_durationmetrictimer.h>
#include <buildboxcommonmetrics_metricguard.h>

#include <set>
#include <thread>

#define TIMER_NAME_COMPILER_DEPS "recc.compiler_deps"
#define TIMER_NAME_BUILD_MERKLE_TREE "recc.build_merkle_tree"

namespace BloombergLP {
namespace recc {

std::mutex ContainerWriteMutex;
std::mutex LogWriteMutex;

proto::Command ActionBuilder::populateCommandProto(
    const std::vector<std::string> &command,
    const std::set<std::string> &outputFiles,
    const std::set<std::string> &outputDirectories,
    const std::map<std::string, std::string> &remoteEnvironment,
    const std::map<std::string, std::string> &platformProperties,
    const std::string &workingDirectory)
{
    proto::Command commandProto;

    for (const auto &arg : command) {
        commandProto.add_arguments(arg);
    }

    for (const auto &envIter : remoteEnvironment) {
        auto envVar = commandProto.add_environment_variables();
        envVar->set_name(envIter.first);
        envVar->set_value(envIter.second);
    }

    // REAPI v2.1 deprecated the `output_files` and `output_directories` fields
    // of the `Command` message, replacing them with `output_paths`.
    const bool outputPathsSupported =
        Env::configured_reapi_version_equal_to_or_newer_than("2.1");

    for (const auto &file : outputFiles) {
        if (outputPathsSupported) {
            commandProto.add_output_paths(file);
        }
        else {
            commandProto.add_output_files(file);
        }
    }

    for (const auto &directory : outputDirectories) {
        if (outputPathsSupported) {
            commandProto.add_output_paths(directory);
        }
        else {
            commandProto.add_output_directories(directory);
        }
    }

    for (const auto &platformIter : platformProperties) {
        auto property = commandProto.mutable_platform()->add_properties();
        property->set_name(platformIter.first);
        property->set_value(platformIter.second);
    }

    commandProto.set_working_directory(workingDirectory);

    return commandProto;
}

std::string
ActionBuilder::commonAncestorPath(const DependencyPairs &dependencies,
                                  const std::set<std::string> &products,
                                  const std::string &workingDirectory)
{
    int parentsNeeded = 0;
    for (const auto &dep : dependencies) {
        parentsNeeded = std::max(parentsNeeded,
                                 FileUtils::parentDirectoryLevels(dep.second));
    }

    for (const auto &product : products) {
        parentsNeeded =
            std::max(parentsNeeded, FileUtils::parentDirectoryLevels(product));
    }

    return FileUtils::lastNSegments(workingDirectory, parentsNeeded);
}

std::string
ActionBuilder::prefixWorkingDirectory(const std::string &workingDirectory,
                                      const std::string &prefix)
{
    if (prefix.empty()) {
        return workingDirectory;
    }

    return prefix + "/" + workingDirectory;
}

void addFileToMerkleTreeHelper(const PathRewritePair &dep_paths,
                               const std::string &cwd,
                               NestedDirectory *nestedDirectory,
                               digest_string_umap *digest_to_filecontents)
{
    // If this path is relative, prepend the remote cwd to it
    // and normalize it, getting rid of any '../' present
    std::string merklePath(dep_paths.second);
    if (merklePath[0] != '/' && !cwd.empty()) {
        merklePath = cwd + "/" + merklePath;
    }
    merklePath = buildboxcommon::FileUtils::normalizePath(merklePath.c_str());

    // don't include a dependency if it's exclusion is requested
    if (FileUtils::hasPathPrefixes(merklePath, RECC_DEPS_EXCLUDE_PATHS)) {
        const std::lock_guard<std::mutex> lock(LogWriteMutex);
        BUILDBOX_LOG_DEBUG("Skipping \"" << merklePath << "\"");
        return;
    }

    std::shared_ptr<ReccFile> file =
        ReccFileFactory::createFile(dep_paths.first.c_str());
    if (!file) {
        const std::lock_guard<std::mutex> lock(LogWriteMutex);
        BUILDBOX_LOG_DEBUG("Encountered unsupported file \""
                           << dep_paths.first << "\", skipping...");
        return;
    }

    {
        const std::lock_guard<std::mutex> lock(ContainerWriteMutex);
        // All necessary merkle path path transformations have already been
        // applied, don't have nestedDirectory apply any additional ones.
        nestedDirectory->add(file, merklePath.c_str(), true);
        (*digest_to_filecontents)[file->getDigest().SerializeAsString()] = file->getFileContents();
    }
}

void ActionBuilder::buildMerkleTree(DependencyPairs &dependency_paths,
                                    const std::string &cwd,
                                    NestedDirectory *nestedDirectory,
                                    digest_string_umap *digest_to_filecontents)
{ // Timed function
    buildboxcommon::buildboxcommonmetrics::MetricGuard<
        buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
        mt(TIMER_NAME_BUILD_MERKLE_TREE);

    BUILDBOX_LOG_DEBUG("Building Merkle tree");

    std::function<void(DependencyPairs::iterator, DependencyPairs::iterator)>
        createMerkleTreeFromIterators = [&](DependencyPairs::iterator start,
                                            DependencyPairs::iterator end) {
            for (; start != end; ++start) {
                addFileToMerkleTreeHelper(*start, cwd, nestedDirectory,
                                          digest_to_filecontents);
            }
        };
    ThreadUtils::parallelizeContainerOperations(dependency_paths,
                                                createMerkleTreeFromIterators);
}

void ActionBuilder::getDependencies(const ParsedCommand &command,
                                    std::set<std::string> *dependencies,
                                    std::set<std::string> *products)
{

    BUILDBOX_LOG_DEBUG("Getting dependencies using the command:");
    if (RECC_VERBOSE == true) {
        std::ostringstream dep_command;
        for (auto &depc : command.get_dependencies_command()) {
            dep_command << depc << " ";
        }
        BUILDBOX_LOG_DEBUG(dep_command.str());
    }

    CommandFileInfo fileInfo;
    { // Timed block
        buildboxcommon::buildboxcommonmetrics::MetricGuard<
            buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
            mt(TIMER_NAME_COMPILER_DEPS);
        fileInfo = Deps::get_file_info(command);
    }

    *dependencies = fileInfo.d_dependencies;

    if (RECC_OUTPUT_DIRECTORIES_OVERRIDE.empty() &&
        RECC_OUTPUT_FILES_OVERRIDE.empty()) {
        *products = fileInfo.d_possibleProducts;
    }
}

std::shared_ptr<proto::Action>
ActionBuilder::BuildAction(const ParsedCommand &command,
                           const std::string &cwd, digest_string_umap *blobs,
                           digest_string_umap *digest_to_filecontents)
{

    if (!command.is_compiler_command() && !RECC_FORCE_REMOTE) {
        return nullptr;
    }

    // According to the REAPI:
    // "[...] the path to the executable [...] must be either a relative
    // path, in which case it is evaluated with respect to the input root,
    // or an absolute path."
    const auto executableName = command.get_command().front();
    if (executableName.find('/') == std::string::npos) {
        throw std::invalid_argument("Command does not contain a relative or "
                                    "absolute path to an executable");
    }

    std::string commandWorkingDirectory;
    NestedDirectory nestedDirectory;

    std::set<std::string> products = RECC_OUTPUT_FILES_OVERRIDE;
    if (!RECC_DEPS_DIRECTORY_OVERRIDE.empty()) {
        BUILDBOX_LOG_DEBUG("Building Merkle tree using directory override");
        // when RECC_DEPS_DIRECTORY_OVERRIDE is set, we will not follow
        // symlinks to help us avoid getting into endless loop
        nestedDirectory =
            make_nesteddirectory(RECC_DEPS_DIRECTORY_OVERRIDE.c_str(),
                                 digest_to_filecontents, false);
        commandWorkingDirectory = RECC_WORKING_DIR_PREFIX;
    }
    else {
        std::set<std::string> deps;
        if (RECC_DEPS_OVERRIDE.empty() && !RECC_FORCE_REMOTE) {
            try {
                getDependencies(command, &deps, &products);
            }
            catch (const subprocess_failed_error &) {
                BUILDBOX_LOG_DEBUG("Running locally to display the error.");
                return nullptr;
            }
        }
        else {
            deps = RECC_DEPS_OVERRIDE;
        }
        // Go through all the dependencies and apply any required path
        // transformations, constructing DependencyParis
        // corresponding to filesystem path -> transformed merkle tree path
        DependencyPairs dep_path_pairs;
        for (const auto &dep : deps) {
            std::string modifiedDep(dep);
            if (modifiedDep[0] == '/') {
                modifiedDep = FileUtils::resolvePathFromPrefixMap(modifiedDep);
                if (FileUtils::hasPathPrefix(modifiedDep, RECC_PROJECT_ROOT)) {
                    modifiedDep = buildboxcommon::FileUtils::makePathRelative(
                        modifiedDep, cwd);
                }
                BUILDBOX_LOG_DEBUG("Mapping local path: ["
                                   << dep << "] to remote path: ["
                                   << modifiedDep << "]");
            }
            dep_path_pairs.push_back(std::make_pair(dep, modifiedDep));
        }

        const auto commonAncestor =
            commonAncestorPath(dep_path_pairs, products, cwd);
        commandWorkingDirectory =
            prefixWorkingDirectory(commonAncestor, RECC_WORKING_DIR_PREFIX);

        buildMerkleTree(dep_path_pairs, commandWorkingDirectory,
                        &nestedDirectory, digest_to_filecontents);
    }

    if (!commandWorkingDirectory.empty()) {
        commandWorkingDirectory = buildboxcommon::FileUtils::normalizePath(
            commandWorkingDirectory.c_str());
        // All necessary merkle path path transformations have already been
        // applied, don't have nestedDirectory apply any additional ones.
        nestedDirectory.addDirectory(commandWorkingDirectory.c_str(), true);
    }

    for (const auto &product : products) {
        if (!product.empty() && product[0] == '/') {
            BUILDBOX_LOG_DEBUG(
                "Command produces file in a location unrelated "
                "to the current directory, so running locally.");
            BUILDBOX_LOG_DEBUG(
                "(use RECC_OUTPUT_[FILES|DIRECTORIES]_OVERRIDE to "
                "override)");
            return nullptr;
        }
    }

    const auto directoryDigest = nestedDirectory.to_digest(blobs);

    const proto::Command commandProto = generateCommandProto(
        command.get_command(), products, RECC_OUTPUT_DIRECTORIES_OVERRIDE,
        RECC_REMOTE_ENV, RECC_REMOTE_PLATFORM, commandWorkingDirectory);
    BUILDBOX_LOG_DEBUG("Command: " << commandProto.ShortDebugString());

    const auto commandDigest = DigestGenerator::make_digest(commandProto);
    (*blobs)[commandDigest.SerializeAsString()] = commandProto.SerializeAsString();

    proto::Action action;
    action.mutable_command_digest()->CopyFrom(commandDigest);
    action.mutable_input_root_digest()->CopyFrom(directoryDigest);
    action.set_do_not_cache(RECC_ACTION_UNCACHEABLE);

    // REAPI v2.2 allows setting the platform property list in the `Action`
    // message, which allows servers to immediately read it without having to
    // dereference the corresponding `Command`.
    if (Env::configured_reapi_version_equal_to_or_newer_than("2.2")) {
        action.mutable_platform()->CopyFrom(commandProto.platform());
    }

    return std::make_shared<proto::Action>(action);
}

build::bazel::remote::execution::v2::Command
ActionBuilder::generateCommandProto(
    const std::vector<std::string> &command,
    const std::set<std::string> &products,
    const std::set<std::string> &outputDirectories,
    const std::map<std::string, std::string> &remoteEnvironment,
    const std::map<std::string, std::string> &platformProperties,
    const std::string &workingDirectory)
{
    // If dependency paths aren't absolute, they are made absolute by
    // having the CWD prepended, and then normalized and replaced. If that
    // is the case, and the CWD contains a replaced prefix, then replace
    // it.
    const auto resolvedWorkingDirectory =
        FileUtils::resolvePathFromPrefixMap(workingDirectory);

    return ActionBuilder::populateCommandProto(
        command, products, outputDirectories, remoteEnvironment,
        platformProperties, resolvedWorkingDirectory);
}

} // namespace recc
} // namespace BloombergLP
