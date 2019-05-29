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

#include <stdexcept>

#include <reccmetrics/durationmetrictimer.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {

DurationMetricTimer::DurationMetricTimer(const std::string &name)
    : d_name(name), d_done(false){};

void DurationMetricTimer::start()
{
    if (d_done) {
        throw std::logic_error("Tried to restart DurationMetricTimer [" +
                               name() +
                               "] that has "
                               "already been stopped.");
    }
    d_start = SteadyClock::now();
}
void DurationMetricTimer::stop()
{
    if (d_done) {
        throw std::logic_error("Tried to stop DurationMetricTimer [" + name() +
                               "] that has "
                               "already been stopped.");
    }
    d_done = true;
    const TimePoint end = SteadyClock::now();
    d_value.setValue(std::chrono::duration_cast<ValueType::TimeDenomination>(
        end - d_start));
}

DurationMetricTimer::ValueType DurationMetricTimer::value() const
{
    return d_value;
}

const std::string &DurationMetricTimer::name() const { return d_name; }

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP