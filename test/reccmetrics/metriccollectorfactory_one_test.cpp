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

TEST(ReccmetricsTest, MetricCollectorFactoryGetSingleCollectorTest)
{
    MetricCollectorFactory *instance = MetricCollectorFactory::getInstance();

    MetricCollector<DurationMetricValue> *durationMetricCollector =
        instance->getCollector<DurationMetricValue>();

    EXPECT_EQ(0, MetricCollectorFactory::getCollector<DurationMetricValue>()
                     ->getIterableContainer()
                     ->size());

    DurationMetricValue myValue1;
    durationMetricCollector->store("metric-1", myValue1);

    EXPECT_EQ(1, MetricCollectorFactory::getCollector<DurationMetricValue>()
                     ->getIterableContainer()
                     ->size());

    DurationMetricValue myValue2;
    durationMetricCollector->store("metric-2", myValue2);

    EXPECT_EQ(2, MetricCollectorFactory::getCollector<DurationMetricValue>()
                     ->getIterableContainer()
                     ->size());
}
