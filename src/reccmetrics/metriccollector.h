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

#ifndef INCLUDED_RECCMETRICS_METRICCOLLECTOR_H
#define INCLUDED_RECCMETRICS_METRICCOLLECTOR_H

#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {

/**
 * MetricCollector
 *
 * Collects the values of Metrics of type ValueType by name
 * and aggregates them if ValueType isAggregatable.
 *
 */
template <class ValueType> class MetricCollector {
  private:
    std::unordered_map<std::string, ValueType> d_metrics;
    std::mutex d_metrics_mutex;

  public:
    // Constructors
    MetricCollector<ValueType>(){};

    // Destructor
    ~MetricCollector<ValueType>(){};

    // Store this in our internal map...
    // TODO When we support c++17 we can easily make this into one function and
    // use if constexpr()

    // Template function for ValueTypes that are Aggregatable
    template <typename... D, typename Q = ValueType>
    typename std::enable_if<Q::isAggregatable>::type
    store(const std::string &name, const ValueType value)
    {
        static_assert(sizeof...(D) == 0, "Do not specify template arguments!");
        std::lock_guard<std::mutex> lock(d_metrics_mutex);
        d_metrics[name] += value;
    }

    // Template function for ValueTypes that are NOT Aggregatable
    template <typename... D, typename Q = ValueType>
    typename std::enable_if<!Q::isAggregatable>::type
    store(const std::string &name, const ValueType value)
    {
        static_assert(sizeof...(D) == 0, "Do not specify template arguments!");
        std::lock_guard<std::mutex> lock(d_metrics_mutex);
        d_metrics[name] = value;
    }
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
