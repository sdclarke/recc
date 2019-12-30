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

#include <authsession_fixture.h>
#include <fileutils.h>
#include <jsonfilemanager.h>

#include <gtest/gtest.h>
#include <iostream>

using BloombergLP::recc::FileUtils;
using BloombergLP::recc::JsonFileManager;

void AuthSessionFiles::SetUpTestCase()
{
    const std::string currDir = FileUtils::getCurrentWorkingDirectory();
    s_clientFilePath = currDir + "/" + "clientToken";

    s_clientFile = new JsonFileManager(s_clientFilePath);

    s_clientFileContents = "{\"access_token\": \"old_fake\", "
                           "\"refresh_token\": \"old_fake\"}";
    s_clientFile->write(s_clientFileContents);
}

void AuthSessionFiles::TearDownTestCase()
{
    if (s_clientFile) {
        s_clientFile->write(s_clientFileContents);
        delete s_clientFile;
    }

    s_clientFile = NULL;
}

void AuthSessionFiles::SetUp() { s_clientFile->write(s_clientFileContents); }

void AuthSessionFiles::TearDown()
{
    s_clientFile->write(s_clientFileContents);
}

// Needed since these members are all static
JsonFileManager *AuthSessionFiles::s_clientFile = NULL;
std::string AuthSessionFiles::s_clientFilePath = "";

std::string AuthSessionFiles::s_clientFileContents = "";
