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

#ifndef INCLUDED_RECCMETRICS_STATSDPUBLISHER_H
#define INCLUDED_RECCMETRICS_STATSDPUBLISHER_H

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <reccmetrics/filewriter.h>
#include <reccmetrics/metriccollectorfactory.h>
#include <reccmetrics/statsdpublisheroptions.h>
#include <reccmetrics/udpwriter.h>

namespace BloombergLP {
namespace recc {
namespace reccmetrics {
/**
 * StatsDPublisher
 */
template <class... ValueTypeList> class StatsDPublisher {
  public:
    // Constructors
    // Specific Constructor
    //  Use given MetricCollectorFactory and PublishMethod/PublishPath
    explicit StatsDPublisher(
        const StatsDPublisherOptions::PublishMethod &publishMethod =
            StatsDPublisherOptions::PublishMethod::StdErr,
        const std::string &publishPath = "", int publishPort = 0,
        MetricCollectorFactory *metricCollectorFactory =
            MetricCollectorFactory::getInstance())
        : d_metricCollectorFactory(metricCollectorFactory),
          d_publishMethod(publishMethod), d_publishPath(publishPath),
          d_publishPort(publishPort)
    {
        switch (d_publishMethod) {
            case StatsDPublisherOptions::PublishMethod::File: {
                if (publishPath.size() == 0) {
                    throw std::runtime_error(
                        "StatsD Publish Method set to `File` but `filePath` "
                        "provided is empty.");
                }
            } break;
            case StatsDPublisherOptions::PublishMethod::UDP: {
                if (publishPath.size() == 0 || publishPort <= 0) {
                    throw std::runtime_error(
                        "StatsD Publish Method set to `UDP` but `path=[" +
                        publishPath + "]`, `port=[" +
                        std::to_string(publishPort) + "]`");
                }
            } break;
            case StatsDPublisherOptions::PublishMethod::StdErr:
                // No special checks here
                break;
        }
    }

    void publish()
    {
        gatherStatsDFromNextCollectors<ValueTypeList...>();

        // Figure out config
        switch (d_publishMethod) {
            case StatsDPublisherOptions::PublishMethod::StdErr: {
                std::cerr << "recc Metrics:\n";
                for (const std::string &metric : d_statsDMetrics) {
                    std::cerr << metric << "\n";
                }
            } break;
            case StatsDPublisherOptions::PublishMethod::File: {
                FileWriter fileWriter(d_publishPath);
                for (const std::string &metric : d_statsDMetrics) {
                    fileWriter.write(metric + "\n");
                }
            } break;
            case StatsDPublisherOptions::PublishMethod::UDP: {
                UDPWriter udpWriter(d_publishPort, d_publishPath);
                for (const std::string &metric : d_statsDMetrics) {
                    udpWriter.write(metric + "\n");
                }
            } break;
        }
    }

  private:
    const MetricCollectorFactory *d_metricCollectorFactory;
    const StatsDPublisherOptions::PublishMethod d_publishMethod;
    const std::string d_publishPath;
    const int d_publishPort;
    std::vector<std::string> d_statsDMetrics;

    // For a single ValueType, store all metrics from beginning to end of
    // iterable
    template <class ValueType>
    void
    gatherStatsDFromValueTypeCollector(MetricCollector<ValueType> *collector)
    {
        auto *collectorContainer = collector->getIterableContainer();

        auto from = collectorContainer->begin();
        const auto to = collectorContainer->end();

        for (; from != to; from++) {
            const std::string &metricName = from->first;
            const ValueType &metricValue = from->second;
            d_statsDMetrics.push_back(metricValue.toStatsD(metricName));
        }
    }

    // Template Functions that call gatherStatsDFromValueTypeCollector
    //                              for everything in the Parameter Pack
    // Those are recursively expanded by the compiler
    // 1 Template parameter
    template <class ValueType> void gatherStatsDFromNextCollectors()
    {
        gatherStatsDFromValueTypeCollector<ValueType>(
            d_metricCollectorFactory->getCollector<ValueType>());
    }
    // >= 2 Template parameters
    template <class ValueType, class NextValueType, class... Rest>
    void gatherStatsDFromNextCollectors()
    {
        gatherStatsDFromValueTypeCollector<ValueType>(
            d_metricCollectorFactory->getCollector<ValueType>());
        gatherStatsDFromNextCollectors<NextValueType, Rest...>();
    }
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
