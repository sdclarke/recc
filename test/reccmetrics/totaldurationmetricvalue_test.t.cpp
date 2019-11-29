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

#include <gtest/gtest.h>
#include <reccmetrics/totaldurationmetricvalue.h>

using namespace BloombergLP::recc::reccmetrics;

TEST(ReccmetricsTest, DurationMetricValueConstructorSetGet)
{
    TotalDurationMetricValue myValue(std::chrono::microseconds(5));
    EXPECT_EQ(myValue.value(), std::chrono::microseconds(5));
}

TEST(ReccmetricsTest, TotalDurationMetricValueSetGet)
{
    TotalDurationMetricValue myValue;
    myValue.setValue(std::chrono::microseconds(2));
    EXPECT_EQ(myValue.value(), std::chrono::microseconds(2));
}

TEST(ReccmetricsTest, TotalDurationMetricValueSetGetStatsD)
{
    TotalDurationMetricValue myValue(std::chrono::microseconds(2000));
    EXPECT_EQ(myValue.toStatsD("my-metric"), "my-metric:2|ms");
}

TEST(ReccmetricsTest, DurationMetricValueAdd)
{
    TotalDurationMetricValue myValue(std::chrono::microseconds(5));
    TotalDurationMetricValue anotherValue(std::chrono::microseconds(10));

    myValue += anotherValue;
    EXPECT_EQ(myValue.value(), std::chrono::microseconds(15));
}
