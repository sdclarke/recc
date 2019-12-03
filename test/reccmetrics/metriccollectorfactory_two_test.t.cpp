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
#include <reccmetrics/metriccollectorfactory.h>
#include <reccmetrics/totaldurationmetricvalue.h>

using namespace BloombergLP::recc::reccmetrics;

TEST(ReccmetricsTest, MetricCollectorFactoryGetTwoCollectorsTest)
{
    TotalDurationMetricValue myValue2;
    MetricCollectorFactory::getCollector<TotalDurationMetricValue>()->store(
        "metric-4", myValue2);

    EXPECT_EQ(0, MetricCollectorFactory::getCollector<DurationMetricValue>()
                     ->getIterableContainer()
                     ->size());

    EXPECT_EQ(1,
              MetricCollectorFactory::getCollector<TotalDurationMetricValue>()
                  ->getIterableContainer()
                  ->size());
}
