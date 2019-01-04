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

#include <recccounterguard.h>

#include <build/bazel/remote/execution/v2/remote_execution_mock.grpc.pb.h>
#include <gmock/gmock.h>
#include <google/bytestream/bytestream_mock.grpc.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace testing;

TEST(ReccCounterGuardTest, NoLimitInitialization)
{
    ReccCounterGuard counterGuard(ReccCounterGuard::s_NO_LIMIT);
    EXPECT_TRUE(counterGuard.is_unlimited());
    EXPECT_TRUE(counterGuard.is_allowed_more());
}

TEST(ReccCounterGuardTest, InvalidLimitInitialization)
{
    EXPECT_THROW(ReccCounterGuard{0}, std::invalid_argument);
    EXPECT_THROW(ReccCounterGuard{-10}, std::invalid_argument);
}

TEST(ReccCounterGuardTest, Limit1)
{
    ReccCounterGuard counterGuard(1);

    EXPECT_FALSE(counterGuard.is_unlimited());

    EXPECT_EQ(1, counterGuard.get_limit());
    EXPECT_TRUE(counterGuard.is_allowed_more());

    EXPECT_NO_THROW(counterGuard.decrease_limit());

    EXPECT_EQ(0, counterGuard.get_limit());
    EXPECT_FALSE(counterGuard.is_allowed_more());
    EXPECT_THROW(counterGuard.decrease_limit(), std::logic_error);
}

TEST(ReccCounterGuardTest, Limit2)
{
    ReccCounterGuard counterGuard(2);
    EXPECT_FALSE(counterGuard.is_unlimited());

    EXPECT_EQ(2, counterGuard.get_limit());
    EXPECT_TRUE(counterGuard.is_allowed_more());
    EXPECT_NO_THROW(counterGuard.decrease_limit());

    EXPECT_EQ(1, counterGuard.get_limit());
    EXPECT_TRUE(counterGuard.is_allowed_more());
    EXPECT_NO_THROW(counterGuard.decrease_limit());

    EXPECT_EQ(0, counterGuard.get_limit());
    EXPECT_FALSE(counterGuard.is_allowed_more());
    EXPECT_THROW(counterGuard.decrease_limit(), std::out_of_range);
}

TEST(ReccCounterGuardTest, LimitFromArgs)
{
    int no_limit = ReccCounterGuard::s_NO_LIMIT;
    EXPECT_EQ(no_limit, ReccCounterGuard::get_limit_from_args(0));

    EXPECT_EQ(no_limit, ReccCounterGuard::get_limit_from_args(-1));

    EXPECT_EQ(no_limit, ReccCounterGuard::get_limit_from_args(-2));
    EXPECT_EQ(no_limit, ReccCounterGuard::get_limit_from_args(-100));

    EXPECT_EQ(1, ReccCounterGuard::get_limit_from_args(1));
    EXPECT_EQ(2, ReccCounterGuard::get_limit_from_args(2));
    EXPECT_EQ(100, ReccCounterGuard::get_limit_from_args(100));
}
