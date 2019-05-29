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

#ifndef INCLUDED_RECCMETRICS_TOTALDURATIONMETRICTIMER_H
#define INCLUDED_RECCMETRICS_TOTALDURATIONMETRICTIMER_H

#include <chrono>
#include <string>
#include <type_traits>

#include <reccmetrics/totaldurationmetricvalue.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {

/**
 *  TotalDurationMetricTimer
 *
 *  A timer that uses TotalDurationMetricValue (aka ValueType) to store its
 * value in
 */
class TotalDurationMetricTimer {
    typedef std::chrono::steady_clock SteadyClock;
    // 'SteadyClock' is an alias representing a monotonic clock (i.e. time
    // points of this clock cannot decrease as physical time moves forward,
    // and, the tick time between ticks of this clock is constant).

    typedef std::chrono::time_point<SteadyClock> TimePoint;
    // 'TimePoint' is an alias representing a point in time measured by a
    // SteadyClock

  private:
    typedef TotalDurationMetricValue ValueType;
    bool d_done;
    TimePoint d_start;
    ValueType d_value;
    const std::string d_name;

  public:
    explicit TotalDurationMetricTimer(const std::string &name);
    ValueType value() const;
    void start();
    void stop();
    const std::string &name() const;
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
