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

#include <env.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;

TEST(EnvTest, DifferentActionCacheServerTest)
{
    const char *testEnviron[] = {
        "RECC_SERVER=somehost:1234", "RECC_CAS_SERVER=someotherhost:5678",
        "RECC_ACTION_CACHE_SERVER=actioncachehost:9999", nullptr};
    std::string expectedActionCacheServer = "actioncachehost:9999";

    parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    handle_special_defaults();

    EXPECT_EQ(expectedActionCacheServer, RECC_ACTION_CACHE_SERVER);
}
