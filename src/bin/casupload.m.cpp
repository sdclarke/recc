﻿// Copyright 2019 Bloomberg Finance L.P
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

#include <casclient.h>
#include <env.h>
#include <fileutils.h>
#include <grpccontext.h>
#include <merklize.h>
#include <reccfile.h>

#include <buildboxcommon_logging.h>
#include <buildboxcommon_systemutils.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <reccdefaults.h>
#include <sys/stat.h>

using namespace BloombergLP::recc;

const std::string
    USAGE("USAGE: casupload --cas-server=ADDRESS [--instance=INSTANCE] "
          "[--follow-symlinks | -f] [--dry-run | -d] "
          "[--output-digest-file=<FILE>] <paths>\n");

const std::string HELP(
    USAGE +
    "Uploads the given files and directories to CAS, then prints the digest "
    "hash and size of\n"
    "the corresponding Directory messages.\n"
    "\n"
    "The files are placed in CAS subdirectories corresponding to their\n"
    "paths. For example, 'casupload file1.txt subdir/file2.txt' would create\n"
    "a CAS directory containing file1.txt and a subdirectory called 'subdir'\n"
    "containing file2.txt.\n"
    "\n"
    "The directories will be uploaded individually as merkle trees.\n"
    "The merkle tree for a directory will contain all of the content\n"
    "within the directory.\n"
    "\n"
    "The server and instance to write to are controlled by the "
    "ADDRESS\n"
    "and INSTANCE arguments.\n"
    "\n"
    "By default 'casupload' will not follow symlinks. Use option -f or \n"
    "'--links' to alter this behavior\n"
    "\n"
    "If `--dry-run` is set, digests will be calculated and printed but \n"
    "no transfers to the remote will take place.\n"
    "\n"
    "If `--output-digest-file=<FILE>` is set, the output digest will be \n"
    "written to <FILE> in the form \"<HASH>/<SIZE_BYTES>\".");

void uploadDirectory(const std::string &path, const proto::Digest &digest,
                     const digest_string_umap &directoryBlobs,
                     const digest_string_umap &directoryDigestToFilecontents,
                     const std::unique_ptr<CASClient> &casClient)
{
    assert(casClient != nullptr);

    try {
        BUILDBOX_LOG_DEBUG("Starting to upload merkle tree...");
        casClient->upload_resources(directoryBlobs,
                                    directoryDigestToFilecontents);
        BUILDBOX_LOG_INFO("Uploaded \"" << path << "\": " << digest.hash()
                                        << "/" << digest.size_bytes());
    }
    catch (const std::runtime_error &e) {
        BUILDBOX_LOG_ERROR("Uploading " << path
                                        << " failed with error: " << e.what());
        exit(1);
    }
}

void processDirectory(const std::string &path, const bool followSymlinks,
                      const std::unique_ptr<CASClient> &casClient)
{
    digest_string_umap directoryBlobs;
    digest_string_umap directoryDigestToFilecontents;

    // set project root to the fully resolved path of this directory
    // to ensure it's the root in the merkle tree
    const std::string abspath = buildboxcommon::FileUtils::makePathAbsolute(
        path, buildboxcommon::SystemUtils::get_current_working_directory());
    RECC_PROJECT_ROOT = abspath.c_str();
    const auto singleNestedDirectory = make_nesteddirectory(
        abspath.c_str(), &directoryDigestToFilecontents, followSymlinks);
    const auto digest = singleNestedDirectory.to_digest(&directoryBlobs);

    BUILDBOX_LOG_DEBUG("Finished building nested directory from \""
                       << path << "\": " << digest.hash() << "/"
                       << digest.size_bytes());
    BUILDBOX_LOG_DEBUG(singleNestedDirectory);

    if (casClient == nullptr) {
        BUILDBOX_LOG_INFO("Computed directory digest for \""
                          << path << "\": " << digest.hash() << "/"
                          << digest.size_bytes());
    }
    else {
        uploadDirectory(path, digest, directoryBlobs,
                        directoryDigestToFilecontents, casClient);
    }
}

struct stat getStatOrExit(const bool followSymlinks, const std::string &path)
{
    try {
        return FileUtils::getStat(path, followSymlinks);
    }
    catch (const std::system_error &) {
        exit(1); // `get_stat()` logged the error.
    }
}

void processPath(const std::string &path, const bool followSymlinks,
                 NestedDirectory *nestedDirectory,
                 digest_string_umap *digestToFileContents,
                 const std::unique_ptr<CASClient> &casClient)
{
    BUILDBOX_LOG_DEBUG("Starting to process \""
                       << path << "\", followSymlinks = " << std::boolalpha
                       << followSymlinks << std::noboolalpha);

    const struct stat statResult = getStatOrExit(followSymlinks, path);

    if (S_ISDIR(statResult.st_mode)) {
        processDirectory(path, followSymlinks, casClient);
        return;
    }

    const std::shared_ptr<ReccFile> file =
        ReccFileFactory::createFile(path.c_str(), followSymlinks);
    if (!file) {
        BUILDBOX_LOG_DEBUG("Encountered unsupported file \""
                           << path << "\", skipping...");
        return;
    }

    nestedDirectory->add(file, path.c_str());
    digestToFileContents->emplace(file->getDigest(), file->getFileContents());
}

int main(int argc, char *argv[])
{
    buildboxcommon::logging::Logger::getLoggerInstance().initialize(argv[0]);

    Env::set_config_locations();
    Env::parse_config_variables();

    if (argc == 1) {
        BUILDBOX_LOG_ERROR(USAGE);
        BUILDBOX_LOG_ERROR("(run \"casupload --help\" for details)");
        return 1;
    }

    // User-provided arguments:
    bool followSymlinks = false;
    bool dryRunMode = false;             // If set, do not upload contents.
    std::string output_digest_file = ""; // Output the digest to this file
    std::vector<std::string> paths;
    std::string casServerAddress, instance;

    for (auto i = 1; i < argc; i++) {
        const std::string argument_value(argv[i]);
        if (argument_value == "--help" || argument_value == "-h") {
            BUILDBOX_LOG_WARNING(HELP);
            return 1;
        }

        if (argument_value.find("--instance=") == 0) {
            instance = argument_value.substr(strlen("--instance="));
        }
        else if (argument_value.find("--cas-server=") == 0) {
            casServerAddress = argument_value.substr(strlen("--cas-server="));
        }
        else if (argument_value == "--follow-symlinks" ||
                 argument_value == "-f") {
            followSymlinks = true;
        }
        else if (argument_value == "--dry-run" || argument_value == "-d") {
            dryRunMode = true;
        }
        else if (argument_value.rfind("--output-digest-file=", 0) == 0) {
            std::string arg_prefix = "--output-digest-file=";
            output_digest_file = argument_value.substr(arg_prefix.length());
        }
        else {
            paths.push_back(argument_value);
        }
    }

    // gRPC connection objects (we don't initialize them if `dryRunMode` is
    // set):
    std::unique_ptr<GrpcContext> grpcContext;
    std::unique_ptr<CASClient> casClient;

    if (!dryRunMode) {
        if (casServerAddress.empty()) {
            std::cerr << "Error: missing --cas-server argument" << std::endl
                      << std::endl
                      << HELP << std::endl;
            return 1;
        }

        buildboxcommon::ConnectionOptions connectionOptions;
        connectionOptions.d_url = casServerAddress.c_str();
        connectionOptions.d_instanceName = instance.c_str();

        auto grpcChannel = connectionOptions.createChannel();

        grpcContext = std::make_unique<GrpcContext>();

        casClient = std::make_unique<CASClient>(
            grpcChannel, connectionOptions.d_instanceName, grpcContext.get());

        if (RECC_CAS_GET_CAPABILITIES) {
            casClient->setUpFromServerCapabilities();
        }
    }

    NestedDirectory nestedDirectory;
    digest_string_umap digestToFileContents;

    // Upload directories individually, and aggregate files to upload as single
    // merkle tree
    for (const auto &path : paths) {
        processPath(path, followSymlinks, &nestedDirectory,
                    &digestToFileContents, casClient);
    }

    if (digestToFileContents.empty()) {
        return 0;
    }

    BUILDBOX_LOG_DEBUG("Building nested directory structure...");
    digest_string_umap blobs;
    const auto directoryDigest = nestedDirectory.to_digest(&blobs);

    BUILDBOX_LOG_INFO("Computed directory digest: "
                      << directoryDigest.hash() << "/"
                      << directoryDigest.size_bytes());

    if (dryRunMode) {
        return 0;
    }

    try {
        casClient->upload_resources(blobs, digestToFileContents);
        BUILDBOX_LOG_DEBUG("Files uploaded successfully");
        if (output_digest_file.length() > 0) {
            std::ofstream digest_file;
            digest_file.open(output_digest_file);
            digest_file << directoryDigest.hash() << "/"
                        << directoryDigest.size_bytes();
            digest_file.close();
        }
        return 0;
    }
    catch (const std::runtime_error &e) {
        BUILDBOX_LOG_ERROR("Uploading files failed with error: " << e.what());
        return 1;
    }
}
