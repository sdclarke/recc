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

#include <env.h>
#include <stdlib.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace std;

TEST(EnvTest, ENVFROMFILE)
{
    // Make sure outside env doesn't get prioritized
    unsetenv("RECC_SERVER");
    unsetenv("RECC_CAS_SERVER");
    unsetenv("TMPDIR");

    // should be set from file
    string expectedReccServer = "localhost:99999";
    string expectedRecCasServer = "localhost:66666";
    string expectedTMPDIR = "/tmp/dir";

    parse_environment();

    EXPECT_EQ(expectedReccServer, RECC_SERVER);
    EXPECT_EQ(expectedRecCasServer, RECC_CAS_SERVER);
    EXPECT_EQ(expectedTMPDIR, TMPDIR);
}
