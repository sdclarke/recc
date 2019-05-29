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

#include <reccmetrics/totaldurationmetricvalue.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {

TotalDurationMetricValue::TotalDurationMetricValue(TimeDenomination value)
    : d_value(value)
{
}

void TotalDurationMetricValue::setValue(
    TotalDurationMetricValue::TimeDenomination value)
{
    d_value = value;
}

TotalDurationMetricValue::TimeDenomination
TotalDurationMetricValue::value() const
{
    return d_value;
}

const std::string
TotalDurationMetricValue::toStatsD(const std::string &myName) const
{
    return std::string(
        myName + ":" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(value())
                .count()) +
        "|ms");
}

TotalDurationMetricValue &TotalDurationMetricValue::
operator+=(const TotalDurationMetricValue &other)
{
    setValue(value() + other.value());
    return *this;
}
} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
