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

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace std;

TEST(EnvTest, EnvTest)
{
    const char *testEnviron[] = {"RECC_FORCE_REMOTE=1",
                                 "RECC_DEPS_OVERRIDE=oneitem",
                                 "RECC_OUTPUT_FILES_OVERRIDE=one,two,three",
                                 "RECC_REMOTE_ENV_key=val",
                                 "RECC_REMOTE_ENV_anotherkey=anotherval",
                                 "RECC_MAX_CONCURRENT_JOBS=15",
                                 "TMPDIR=/some/tmp/dir",
                                 nullptr};
    set<string> expectedDeps = {"oneitem"};
    set<string> expectedOutputFiles = {"one", "two", "three"};
    map<string, string> expectedRemoteEnv = {{"key", "val"},
                                             {"anotherkey", "anotherval"}};
    parse_environment(testEnviron);

    EXPECT_TRUE(RECC_FORCE_REMOTE);
    EXPECT_EQ(expectedDeps, RECC_DEPS_OVERRIDE);
    EXPECT_EQ(expectedOutputFiles, RECC_OUTPUT_FILES_OVERRIDE);
    EXPECT_EQ(expectedRemoteEnv, RECC_REMOTE_ENV);
    EXPECT_EQ(15, RECC_MAX_CONCURRENT_JOBS);
    EXPECT_EQ("/some/tmp/dir", TMPDIR);
}
