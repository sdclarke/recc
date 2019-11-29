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

#include <formpost.h>

#include <gtest/gtest.h>
#include <string>

using namespace BloombergLP::recc;
using namespace testing;

TEST(FormPost, GenerateFormPost)
{
    FormPost formPostFactory;
    std::string generatedPost = formPostFactory.generate_post("mock_refresh");
    std::string expectedPost = "refresh_token=mock_refresh";
    EXPECT_EQ(expectedPost, generatedPost);
}
