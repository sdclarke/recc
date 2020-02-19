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
#include <buildboxcommonmetrics_durationmetrictimer.h>
#include <buildboxcommonmetrics_metricguard.h>
#include <digestgenerator.h>
#include <env.h>
#include <fileutils.h>
#include <logging.h>
#include <reccdefaults.h>
#include <set>
#include <thread>
#include <threadutils.h>

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

    for (const auto &file : outputFiles) {
        commandProto.add_output_files(file);
    }

    for (const auto &directory : outputDirectories) {
        commandProto.add_output_directories(directory);
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
ActionBuilder::commonAncestorPath(const std::set<std::string> &dependencies,
                                  const std::set<std::string> &products,
                                  const std::string &workingDirectory)
{
    int parentsNeeded = 0;
    for (const auto &dep : dependencies) {
        parentsNeeded =
            std::max(parentsNeeded, FileUtils::parentDirectoryLevels(dep));
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

typedef std::set<std::string>::iterator setIterator;
void addFileToMerkleTreeHelper(const std::string &dep, const std::string &cwd,
                               NestedDirectory *nestedDirectory,
                               digest_string_umap *digest_to_filecontents)
{
    // If the dependency is an absolute path, leave the merklePath
    // untouched
    std::string merklePath = (dep[0] == '/') ? dep : cwd + "/" + dep;
    merklePath = buildboxcommon::FileUtils::normalizePath(merklePath.c_str());

    // don't include a dependency if it's exclusion is requested
    if (FileUtils::hasPathPrefixes(merklePath, RECC_DEPS_EXCLUDE_PATHS)) {
        const std::lock_guard<std::mutex> lock(LogWriteMutex);
        RECC_LOG_VERBOSE("Skipping \"" << merklePath << "\"");
        return;
    }

    std::shared_ptr<ReccFile> file = ReccFileFactory::createFile(dep.c_str());
    if (!file) {
        const std::lock_guard<std::mutex> lock(LogWriteMutex);
        RECC_LOG_VERBOSE("Encountered unsupported file \""
                         << dep << "\", skipping...");
        return;
    }

    {
        const std::lock_guard<std::mutex> lock(ContainerWriteMutex);
        nestedDirectory->add(file, merklePath.c_str());
        (*digest_to_filecontents)[file->getDigest()] = file->getFileContents();
    }
}

void ActionBuilder::buildMerkleTree(const std::set<std::string> &dependencies,
                                    const std::string &cwd,
                                    NestedDirectory *nestedDirectory,
                                    digest_string_umap *digest_to_filecontents)
{ // Timed function
    buildboxcommon::buildboxcommonmetrics::MetricGuard<
        buildboxcommon::buildboxcommonmetrics::DurationMetricTimer>
        mt(TIMER_NAME_BUILD_MERKLE_TREE);

    RECC_LOG_VERBOSE("Building Merkle tree");

    std::function<void(setIterator, setIterator)>
        createMerkleTreeFromIterators =
            [&](setIterator start, setIterator end) {
                for (; start != end; ++start) {
                    addFileToMerkleTreeHelper(*start, cwd, nestedDirectory,
                                              digest_to_filecontents);
                }
            };
    ThreadUtils::parallelizeContainerOperations(dependencies,
                                                createMerkleTreeFromIterators);
}

void ActionBuilder::getDependencies(const ParsedCommand &command,
                                    std::set<std::string> *dependencies,
                                    std::set<std::string> *products)
{
    RECC_LOG_VERBOSE("Getting dependencies");
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
        RECC_LOG_VERBOSE("Building Merkle tree using directory override");
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
                RECC_LOG_VERBOSE("Running locally to display the error.");
                return nullptr;
            }
        }
        else {
            deps = RECC_DEPS_OVERRIDE;
        }

        const auto commonAncestor = commonAncestorPath(deps, products, cwd);
        commandWorkingDirectory =
            prefixWorkingDirectory(commonAncestor, RECC_WORKING_DIR_PREFIX);

        buildMerkleTree(deps, commandWorkingDirectory, &nestedDirectory,
                        digest_to_filecontents);
    }

    if (!commandWorkingDirectory.empty()) {
        commandWorkingDirectory = buildboxcommon::FileUtils::normalizePath(
            commandWorkingDirectory.c_str());
        nestedDirectory.addDirectory(commandWorkingDirectory.c_str());
    }

    for (const auto &product : products) {
        if (!product.empty() && product[0] == '/') {
            RECC_LOG_VERBOSE("Command produces file in a location unrelated "
                             "to the current directory, so running locally.");
            RECC_LOG_VERBOSE(
                "(use RECC_OUTPUT_[FILES|DIRECTORIES]_OVERRIDE to "
                "override)");
            return nullptr;
        }
    }

    const auto directoryDigest = nestedDirectory.to_digest(blobs);

    const proto::Command commandProto = generateCommandProto(
        command.get_command(), products, RECC_OUTPUT_DIRECTORIES_OVERRIDE,
        RECC_REMOTE_ENV, RECC_REMOTE_PLATFORM, commandWorkingDirectory);
    RECC_LOG_VERBOSE("Command: " << commandProto.ShortDebugString());

    const auto commandDigest = DigestGenerator::make_digest(commandProto);
    (*blobs)[commandDigest] = commandProto.SerializeAsString();

    proto::Action action;
    action.mutable_command_digest()->CopyFrom(commandDigest);
    action.mutable_input_root_digest()->CopyFrom(directoryDigest);
    action.set_do_not_cache(RECC_ACTION_UNCACHEABLE);

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
