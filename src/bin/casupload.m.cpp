// Copyright 2019 Bloomberg Finance L.P
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

#include <buildboxcommon_client.h>
#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>
#include <buildboxcommon_merklize.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <sys/stat.h>

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

void uploadResources(
    const buildboxcommon::digest_string_map &blobs,
    const buildboxcommon::digest_string_map &digest_to_filepaths,
    const std::unique_ptr<buildboxcommon::Client> &casClient)
{
    std::vector<buildboxcommon::Digest> digestsToUpload;
    for (const auto &i : blobs) {
        digestsToUpload.push_back(i.first);
    }
    for (const auto &i : digest_to_filepaths) {
        digestsToUpload.push_back(i.first);
    }

    const auto missingDigests = casClient->findMissingBlobs(digestsToUpload);

    std::vector<buildboxcommon::Client::UploadRequest> upload_requests;
    upload_requests.reserve(missingDigests.size());
    for (const auto &digest : missingDigests) {
        // Finding the data in one of the source maps:
        if (blobs.count(digest)) {
            upload_requests.emplace_back(buildboxcommon::Client::UploadRequest(
                digest, blobs.at(digest)));
        }
        else if (digest_to_filepaths.count(digest)) {
            upload_requests.emplace_back(
                buildboxcommon::Client::UploadRequest::from_path(
                    digest, digest_to_filepaths.at(digest)));
        }
        else {
            throw std::runtime_error(
                "FindMissingBlobs returned non-existent digest");
        }
    }

    casClient->uploadBlobs(upload_requests);
}

void uploadDirectory(
    const std::string &path, const buildboxcommon::Digest &digest,
    const buildboxcommon::digest_string_map &directoryBlobs,
    const buildboxcommon::digest_string_map &directoryDigestToFilepaths,
    const std::unique_ptr<buildboxcommon::Client> &casClient)
{
    assert(casClient != nullptr);

    try {
        BUILDBOX_LOG_DEBUG("Starting to upload merkle tree...");
        uploadResources(directoryBlobs, directoryDigestToFilepaths, casClient);
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
                      const std::unique_ptr<buildboxcommon::Client> &casClient)
{
    buildboxcommon::digest_string_map directoryBlobs;
    buildboxcommon::digest_string_map directoryDigestToFilepaths;

    const auto singleNestedDirectory = buildboxcommon::make_nesteddirectory(
        path.c_str(), &directoryDigestToFilepaths, followSymlinks);
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
                        directoryDigestToFilepaths, casClient);
    }
}

void processPath(const std::string &path, const bool followSymlinks,
                 buildboxcommon::NestedDirectory *nestedDirectory,
                 buildboxcommon::digest_string_map *digestToFilePaths,
                 const std::unique_ptr<buildboxcommon::Client> &casClient)
{
    BUILDBOX_LOG_DEBUG("Starting to process \""
                       << path << "\", followSymlinks = " << std::boolalpha
                       << followSymlinks << std::noboolalpha);

    struct stat statResult;
    int statFlags = 0;
    if (!followSymlinks) {
        statFlags = AT_SYMLINK_NOFOLLOW;
    }
    if (fstatat(AT_FDCWD, path.c_str(), &statResult, statFlags) != 0) {
        BUILDBOX_LOG_ERROR("Error getting file status for path \""
                           << path << "\": " << strerror(errno));
        exit(1);
    }

    if (S_ISDIR(statResult.st_mode)) {
        processDirectory(path, followSymlinks, casClient);
    }
    else if (S_ISLNK(statResult.st_mode)) {
        std::string target(static_cast<size_t>(statResult.st_size), '\0');

        if (readlink(path.c_str(), &target[0], target.size()) < 0) {
            BUILDBOX_LOG_ERROR("Error reading target of symbolic link \""
                               << path << "\": " << strerror(errno));
            exit(1);
        }
        nestedDirectory->addSymlink(target, path.c_str());
    }
    else if (S_ISREG(statResult.st_mode)) {
        const buildboxcommon::File file(path.c_str());
        nestedDirectory->add(file, path.c_str());
        digestToFilePaths->emplace(file.d_digest, path);
    }
    else {
        BUILDBOX_LOG_DEBUG("Encountered unsupported file \""
                           << path << "\", skipping...");
    }
}

int main(int argc, char *argv[])
{
    buildboxcommon::logging::Logger::getLoggerInstance().initialize(argv[0]);

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

    // CAS client object (we don't initialize it if `dryRunMode` is set):
    std::unique_ptr<buildboxcommon::Client> casClient;

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

        casClient = std::make_unique<buildboxcommon::Client>();
        casClient->init(connectionOptions);
    }

    buildboxcommon::NestedDirectory nestedDirectory;
    buildboxcommon::digest_string_map digestToFilePaths;

    // Upload directories individually, and aggregate files to upload as single
    // merkle tree
    for (const auto &path : paths) {
        processPath(path, followSymlinks, &nestedDirectory, &digestToFilePaths,
                    casClient);
    }

    if (digestToFilePaths.empty()) {
        return 0;
    }

    BUILDBOX_LOG_DEBUG("Building nested directory structure...");
    buildboxcommon::digest_string_map blobs;
    const auto directoryDigest = nestedDirectory.to_digest(&blobs);

    BUILDBOX_LOG_INFO("Computed directory digest: "
                      << directoryDigest.hash() << "/"
                      << directoryDigest.size_bytes());

    if (dryRunMode) {
        return 0;
    }

    try {
        uploadResources(blobs, digestToFilePaths, casClient);
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
