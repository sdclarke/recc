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

#ifndef INCLUDED_RECCMETRICS_METRICGUARD_H
#define INCLUDED_RECCMETRICS_METRICGUARD_H

#include <string>

#include <reccmetrics/metriccollectorfactory.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {
/**
 * MetricGuard is the Metric Guard class
 *
 * It invokes start() and stop() on MetricType upon creation and destruction
 * of the MetricGuard and forwards the value of the metric ValueType to the
 * appropriate collector (provided by the MetricCollectorFactory)
 */
template <class MetricType> class MetricGuard {
  private:
    // Infer the type of the value of MetricType
    // by inspecting the type MetricType.value() returns
    typedef decltype(std::declval<MetricType>().value()) ValueType;
    bool d_enabled;
    MetricCollector<ValueType> *d_collector;
    MetricType d_metric;

  public:
    MetricGuard(const std::string &name, bool enabled,
                MetricCollector<ValueType> *collector =
                    MetricCollectorFactory::getCollector<ValueType>())
        : d_collector(collector), d_enabled(enabled), d_metric(name)
    {
        if (!enabled)
            return;
        d_metric.start();
    }

    // Destructor
    ~MetricGuard<MetricType>()
    {
        if (!d_enabled)
            return;
        d_metric.stop();
        d_collector->store(d_metric.name(), d_metric.value());
    };
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
