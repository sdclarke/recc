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

#include <casclient.h>
#include <env.h>
#include <merklize.h>

#include <cstdlib>
#include <cstring>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>

using namespace BloombergLP::recc;
using namespace std;

const string HELP(
    "USAGE: casupload <paths>\n"
    "Uploads the given files to CAS, then prints the digest hash and size of\n"
    "the corresponding Directory message.\n"
    "\n"
    "The files are placed in CAS subdirectories corresponding to their\n"
    "paths. For example, 'casupload file1.txt subdir/file2.txt' would create\n"
    "a CAS directory containing file1.txt and a subdirectory called 'subdir'\n"
    "containing file2.txt.\n"
    "\n"
    "The server and instance to write to are controlled by the RECC_SERVER\n"
    "and RECC_INSTANCE environment variables.\n");

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        cerr << "USAGE: casupload <paths>" << endl << endl;
        cerr << "(run \"casupload --help\" for details)" << endl;
        return 1;
    }
    else if (argc == 2 &&
             (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        cerr << HELP;
        return 1;
    }

    parse_environment();

    if (RECC_CAS_SERVER.empty()) {
        if (RECC_SERVER.empty()) {
            RECC_CAS_SERVER = "localhost:8085";
            cerr << "Warning: no RECC_SERVER environment variable specified."
                 << endl;
            cerr << "Using default server (" << RECC_CAS_SERVER << ")" << endl;
        }
        RECC_CAS_SERVER = RECC_SERVER;
    }

    NestedDirectory nestedDirectory;
    unordered_map<proto::Digest, string> blobs;
    unordered_map<proto::Digest, string> filenames;

    for (int i = 1; i < argc; ++i) {
        File file(argv[i]);
        nestedDirectory.add(file, argv[i]);
        filenames[file.digest] = argv[i];
    }

    auto directoryDigest = nestedDirectory.to_digest(&blobs);

    auto channel = grpc::CreateChannel(RECC_CAS_SERVER,
                                       grpc::InsecureChannelCredentials());
    CASClient(channel, RECC_INSTANCE).upload_resources(blobs, filenames);

    cout << directoryDigest.hash() << endl;
    cout << directoryDigest.size_bytes() << endl;

    return 0;
}
