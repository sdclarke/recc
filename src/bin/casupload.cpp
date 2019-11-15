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

#include <authsession.h>
#include <casclient.h>
#include <env.h>
#include <grpcchannels.h>
#include <grpccontext.h>
#include <logging.h>
#include <merklize.h>
#include <reccfile.h>

#include <cstdlib>
#include <cstring>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <reccdefaults.h>
#include <sys/stat.h>

using namespace BloombergLP::recc;

const std::string USAGE(
    "USAGE: casupload  [--follow-symlinks | -f] [--dry-run | -d] <paths>\n");

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
    "RECC_CAS_SERVER\n"
    "and RECC_INSTANCE environment variables.\n"
    "\n"
    "By default 'casupload' will not follow symlinks. Use option -f or \n"
    "'--follow-symlinks' to alter this behavior\n"
    "\n"
    "If `--dry-run` is set, digests will be calculated and printed but \n"
    "no transfers to the remote will take place.\n");

void uploadDirectory(const std::string &path, const proto::Digest &digest,
                     const digest_string_umap &directoryBlobs,
                     const digest_string_umap &directoryDigestToFilecontents,
                     const std::unique_ptr<CASClient> &casClient)
{
    assert(casClient != nullptr);

    try {
        RECC_LOG_VERBOSE("Starting to upload merkle tree...");
        casClient->upload_resources(directoryBlobs,
                                    directoryDigestToFilecontents);
        RECC_LOG("Uploaded \"" << path << "\": " << digest.hash() << "/"
                               << digest.size_bytes());
    }
    catch (const std::runtime_error &e) {
        RECC_LOG_ERROR("Uploading " << path
                                    << " failed with error: " << e.what());
        exit(1);
    }
}

void processDirectory(const std::string &path, const bool followSymlinks,
                      const std::unique_ptr<CASClient> &casClient)
{
    digest_string_umap directoryBlobs;
    digest_string_umap directoryDigestToFilecontents;

    // set root project so the path in the merkle tree
    // starts there
    RECC_PROJECT_ROOT = path.c_str();
    const auto singleNestedDirectory = make_nesteddirectory(
        path.c_str(), &directoryDigestToFilecontents, followSymlinks);
    const auto digest = singleNestedDirectory.to_digest(&directoryBlobs);

    RECC_LOG_VERBOSE("Finished building nested directory from \""
                     << path << "\": " << digest.hash() << "/"
                     << digest.size_bytes());
    RECC_LOG_VERBOSE(singleNestedDirectory);

    if (casClient == nullptr) {
        RECC_LOG("Computed directory digest for \""
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
        return FileUtils::get_stat(path.c_str(), followSymlinks);
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
    RECC_LOG_VERBOSE("Starting to process \""
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
        RECC_LOG_VERBOSE("Encountered unsupported file \""
                         << path << "\", skipping...");
        return;
    }

    nestedDirectory->add(file, path.c_str());
    digestToFileContents->emplace(file->getDigest(), file->getFileContents());
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        RECC_LOG_ERROR(USAGE);
        RECC_LOG_ERROR("(run \"casupload --help\" for details)");
        return 1;
    }

    // User-provided arguments:
    bool followSymlinks = false;
    bool dryRunMode = false; // If set, do not upload contents.
    std::vector<std::string> paths;

    for (auto i = 1; i < argc; i++) {
        const std::string argument_value(argv[i]);

        if (argument_value == "--help" || argument_value == "-h") {
            RECC_LOG_WARNING(HELP);
            return 1;
        }

        if (argument_value == "--follow-symlinks" || argument_value == "-f") {
            followSymlinks = true;
        }
        else if (argument_value == "--dry-run" || argument_value == "-d") {
            dryRunMode = true;
        }
        else {
            paths.push_back(argument_value);
        }
    }

    set_config_locations();
    parse_config_variables();

    // gRPC connection objects (we don't initialize them if `dryRunMode` is
    // set):
    std::unique_ptr<GrpcChannels> returnChannels;
    std::unique_ptr<GrpcContext> grpcContext;
    std::unique_ptr<CASClient> casClient;
    std::unique_ptr<FormPost> formPostFactory;
    std::unique_ptr<AuthSession> reccAuthSession;

    if (!dryRunMode) {
        returnChannels = std::make_unique<GrpcChannels>(
            GrpcChannels::get_channels_from_config());

        grpcContext = std::make_unique<GrpcContext>();
        formPostFactory = std::make_unique<FormPost>();

        if (RECC_SERVER_JWT) {
            reccAuthSession =
                std::make_unique<AuthSession>(formPostFactory.get());
            grpcContext->set_auth(reccAuthSession.get());
        }

        casClient = std::make_unique<CASClient>(
            returnChannels->cas(), RECC_INSTANCE, grpcContext.get());

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

    RECC_LOG_VERBOSE("Building nested directory structure...");
    digest_string_umap blobs;
    const auto directoryDigest = nestedDirectory.to_digest(&blobs);

    RECC_LOG("Computed directory digest: " << directoryDigest.hash() << "/"
                                           << directoryDigest.size_bytes());

    if (dryRunMode) {
        return 0;
    }

    try {
        casClient->upload_resources(blobs, digestToFileContents);
        RECC_LOG("Files uploaded successfully");
        return 0;
    }
    catch (const std::runtime_error &e) {
        RECC_LOG_ERROR("Uploading files failed with error: " << e.what());
        return 1;
    }
}
