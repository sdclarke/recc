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
#include <reccmetrics/totaldurationmetrictimer.h>

using namespace BloombergLP::recc::reccmetrics;

TEST(ReccmetricsTest, TotalDurationMetricTimerName)
{
    TotalDurationMetricTimer timer("my-timer");
    EXPECT_EQ("my-timer", timer.name());
}

TEST(ReccmetricsTest, TotalDurationMetricTimerStart)
{
    TotalDurationMetricTimer timer("my-timer");
    timer.start();
}

TEST(ReccmetricsTest, TotalDurationMetricTimerStartStop)
{
    TotalDurationMetricTimer timer("my-timer");
    timer.start();
    timer.stop();
}

TEST(ReccmetricsTest, TotalDurationMetricTimerStopTwice)
{
    TotalDurationMetricTimer timer("my-timer");
    timer.start();
    timer.stop();
    EXPECT_THROW(timer.stop(), std::logic_error);
}

TEST(ReccmetricsTest, TotalDurationMetricTimerStartStopStart)
{
    TotalDurationMetricTimer timer("my-timer");
    timer.start();
    timer.stop();
    EXPECT_THROW(timer.start();, std::logic_error);
}

TEST(ReccmetricsTest, TotalDurationMetricTimerValue)
{
    TotalDurationMetricTimer timer("my-timer");
    timer.start();
    timer.stop();
    timer.value();
}
