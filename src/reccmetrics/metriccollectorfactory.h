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

#ifndef INCLUDED_RECCMETRICS_METRICCOLLECTORFACTORY_H
#define INCLUDED_RECCMETRICS_METRICCOLLECTORFACTORY_H

#include <string>
#include <typeinfo>
#include <unordered_map>

#include <reccmetrics/metriccollector.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {
/**
 * MetricCollectorFactory
 *
 * Singleton to manage static MetricCollectors of various types <ValueType>
 */
class MetricCollectorFactory {
  private:
    // Constructor
    MetricCollectorFactory(){};
    // Delete Copy Constructor
    MetricCollectorFactory(MetricCollectorFactory const &) = delete;
    // Destructor
    ~MetricCollectorFactory(){};
    // Delete Assignment operator
    MetricCollectorFactory &operator=(MetricCollectorFactory const &) = delete;

  public:
    // Singleton Instance accessor
    static MetricCollectorFactory *getInstance();

    // Singleton Instance Accessor Template for each ValueType
    template <typename ValueType>
    static MetricCollector<ValueType> *getCollector()
    {
        static MetricCollector<ValueType> collector;
        return &collector;
    }
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
