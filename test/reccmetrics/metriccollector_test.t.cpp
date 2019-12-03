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
#include <reccmetrics/durationmetricvalue.h>
#include <reccmetrics/metriccollector.h>
#include <reccmetrics/totaldurationmetricvalue.h>

using namespace BloombergLP::recc::reccmetrics;

TEST(ReccmetricsTest, DurationMetricValueCollectorMultiTest)
{
    MetricCollector<DurationMetricValue> durationMetricCollector;

    DurationMetricValue myValue1;
    durationMetricCollector.store("metric-1", myValue1);

    ASSERT_EQ(1, durationMetricCollector.getIterableContainer()->size());

    DurationMetricValue myValue2;
    durationMetricCollector.store("metric-2", myValue1);

    ASSERT_EQ(2, durationMetricCollector.getIterableContainer()->size());
}

TEST(ReccmetricsTest, DurationMetricValueCollectorUpdateTest)
{
    MetricCollector<DurationMetricValue> durationMetricCollector;

    DurationMetricValue myValue1(std::chrono::microseconds(2));
    durationMetricCollector.store("metric", myValue1);

    // We should only have 1 metric, with the value we instantiated it with
    // above
    ASSERT_EQ(1, durationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(2),
              durationMetricCollector.getIterableContainer()
                  ->find("metric")
                  ->second.value());

    // Since DurationMetricValue is non-aggregatable,
    // storing the metric with the same name should *replace* it with the new
    // value
    DurationMetricValue myValue2(std::chrono::microseconds(5));
    durationMetricCollector.store("metric", myValue2);

    ASSERT_EQ(1, durationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(5),
              durationMetricCollector.getIterableContainer()
                  ->find("metric")
                  ->second.value());
}

TEST(ReccmetricsTest, TotalDurationMetricValueCollectorMultiTest)
{
    MetricCollector<TotalDurationMetricValue> totalDurationMetricCollector;

    TotalDurationMetricValue myValue1;
    totalDurationMetricCollector.store("metric-1", myValue1);

    ASSERT_EQ(1, totalDurationMetricCollector.getIterableContainer()->size());

    TotalDurationMetricValue myValue2;
    totalDurationMetricCollector.store("metric-2", myValue1);

    ASSERT_EQ(2, totalDurationMetricCollector.getIterableContainer()->size());
}

TEST(ReccmetricsTest, TotalDurationMetricValueCollectorAggregateTest)
{
    MetricCollector<TotalDurationMetricValue> totalDurationMetricCollector;

    TotalDurationMetricValue myValue1(std::chrono::microseconds(2));
    totalDurationMetricCollector.store("metric", myValue1);

    ASSERT_EQ(1, totalDurationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(2),
              totalDurationMetricCollector.getIterableContainer()
                  ->find("metric")
                  ->second.value());

    TotalDurationMetricValue myValue2(std::chrono::microseconds(5));
    totalDurationMetricCollector.store("metric", myValue2);
    ASSERT_EQ(1, totalDurationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(7),
              totalDurationMetricCollector.getIterableContainer()
                  ->find("metric")
                  ->second.value());
}

TEST(ReccmetricsTest, TotalDurationMetricValueCollectorMultiAggregateTest)
{
    MetricCollector<TotalDurationMetricValue> totalDurationMetricCollector;

    TotalDurationMetricValue myValue1(std::chrono::microseconds(2));
    totalDurationMetricCollector.store("metric", myValue1);

    ASSERT_EQ(1, totalDurationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(2),
              totalDurationMetricCollector.getIterableContainer()
                  ->find("metric")
                  ->second.value());

    TotalDurationMetricValue myValueOther1(std::chrono::microseconds(4));
    totalDurationMetricCollector.store("metric-other", myValueOther1);
    ASSERT_EQ(2, totalDurationMetricCollector.getIterableContainer()->size());

    TotalDurationMetricValue myValue2(std::chrono::microseconds(5));
    totalDurationMetricCollector.store("metric", myValue2);
    ASSERT_EQ(2, totalDurationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(7),
              totalDurationMetricCollector.getIterableContainer()
                  ->find("metric")
                  ->second.value());

    TotalDurationMetricValue myValueOther2(std::chrono::microseconds(9));
    totalDurationMetricCollector.store("metric-other", myValueOther2);
    ASSERT_EQ(2, totalDurationMetricCollector.getIterableContainer()->size());
    ASSERT_EQ(std::chrono::microseconds(13),
              totalDurationMetricCollector.getIterableContainer()
                  ->find("metric-other")
                  ->second.value());
}
