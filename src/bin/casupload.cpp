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

const std::string HELP(
    "USAGE: casupload <paths> [--followSymlinks]\n"
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
    "'--followSymlinks' to alter this behavior\n");

int main(int argc, char *argv[])
{
    bool followSymlinks = false;
    int pathArgsIndex = 1;
    if (argc <= 1) {
        RECC_LOG_ERROR("USAGE: casupload <paths> [-f | --followSymlinks]");
        RECC_LOG_ERROR("(run \"casupload --help\" for details)");
        return 1;
    }
    else if (argc == 2 &&
             (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        RECC_LOG_WARNING(HELP);
        return 1;
    }
    else if (argc == 3 && (strcmp(argv[1], "--followSymlinks") == 0 ||
                           strcmp(argv[1], "-f") == 0)) {
        followSymlinks = true;
        pathArgsIndex = 2;
    }

    set_config_locations();
    parse_config_variables();

    GrpcChannels returnChannels = GrpcChannels::get_channels_from_config();
    GrpcContext grpcContext;
    std::unique_ptr<AuthSession> reccAuthSession;
    FormPost formPostFactory;
    if (RECC_SERVER_JWT) {
        reccAuthSession.reset(new AuthSession(&formPostFactory));
        grpcContext.set_auth(reccAuthSession.get());
    }

    CASClient casClient(returnChannels.cas(), RECC_INSTANCE, &grpcContext);
    if (RECC_CAS_GET_CAPABILITIES) {
        casClient.setUpFromServerCapabilities();
    }

    NestedDirectory nestedDirectory;
    digest_string_umap digest_to_filecontents;

    // Upload directories individually, and aggregate files to upload as single
    // merkle tree
    for (int i = pathArgsIndex; i < argc; ++i) {
        RECC_LOG_VERBOSE("Starting to process \""
                         << argv[i] << "\", followSymlinks = "
                         << std::boolalpha << followSymlinks);

        struct stat statResult = FileUtils::get_stat(argv[i], followSymlinks);
        if (S_ISDIR(statResult.st_mode)) {
            digest_string_umap directory_blobs;
            digest_string_umap directory_digest_to_filecontents;

            // set root project so the path in the merkle tree
            // starts there
            RECC_PROJECT_ROOT = argv[i];
            auto singleNestedDirectory = make_nesteddirectory(
                argv[i], &directory_digest_to_filecontents, followSymlinks);
            auto digest = singleNestedDirectory.to_digest(&directory_blobs);
            RECC_LOG_VERBOSE("Finished building nested directory from \""
                             << argv[i] << "\"");
            RECC_LOG_VERBOSE(singleNestedDirectory);

            try {
                RECC_LOG_VERBOSE("Starting to upload merkle tree");
                casClient.upload_resources(directory_blobs,
                                           directory_digest_to_filecontents);
            }
            catch (const std::runtime_error &e) {
                RECC_LOG_ERROR("Uploading "
                               << argv[i]
                               << " failed with error: " << e.what());
                exit(1);
            }
            RECC_LOG_VERBOSE("Uploaded " << argv[i] << ": " << digest.hash()
                                         << "/" << digest.size_bytes());
        }
        else {
            std::shared_ptr<ReccFile> file =
                ReccFileFactory::createFile(argv[i], followSymlinks);
            if (!file) {
                RECC_LOG_VERBOSE("Encountered unsupported file \""
                                 << argv[i] << "\", skipping...");
                continue;
            }

            nestedDirectory.add(file, argv[i]);
            digest_to_filecontents.emplace(file->getDigest(),
                                           file->getFileContents());
        }
    }

    if (!digest_to_filecontents.empty()) {
        RECC_LOG_VERBOSE("Building nested directory structure");
        digest_string_umap blobs;
        auto directoryDigest = nestedDirectory.to_digest(&blobs);
        try {
            casClient.upload_resources(blobs, digest_to_filecontents);
        }
        catch (const std::runtime_error &e) {
            RECC_LOG_ERROR("Uploading files failed with error: " << e.what());
            exit(1);
        }
        RECC_LOG_VERBOSE("Uploaded files: " << directoryDigest.hash() << "/"
                                            << directoryDigest.size_bytes());
    }

    return 0;
}
