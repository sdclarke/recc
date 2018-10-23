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

#include <algorithm>
#include <env.h>
#include <gtest/gtest.h>
#include <map>
#include <vector>

using namespace BloombergLP::recc;
using namespace std;

TEST(EnvTest, EnvCheckOrder)
{
    int counter = 5;
    const vector<const char *> expectedOrderVec = {
        "RECC_REMOTE_PLATFORM_arch=x86_64", "RECC_REMOTE_PLATFORM_test=x64_86",
        "RECC_REMOTE_PLATFORM_zed=win10"};

    while (counter) {

        vector<const char *> randomVec = expectedOrderVec;

        // shuffle the vector randomly to test permutations.
        random_shuffle(randomVec.begin(), randomVec.end());

        randomVec.push_back(nullptr);

        parse_config_variables(randomVec.data());

        int v_In = 0;

        // ensure ordering is the same as expected Vector
        for (const auto &platformIter : RECC_REMOTE_PLATFORM) {
            string map_val = "RECC_REMOTE_PLATFORM_" + platformIter.first +
                             "=" + platformIter.second;
            EXPECT_EQ(map_val, string(expectedOrderVec[v_In]));
            ++v_In;
        }
        --counter;
    }
}
