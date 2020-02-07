// Copyright 2018 Bloomberg Finance L.P
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

#include <metricsconfig.h>

namespace BloombergLP {
namespace recc {

StatsDPublisherType get_statsdpublisher_from_config()
{
    buildboxcommon::buildboxcommonmetrics::StatsDPublisherOptions::
        PublishMethod publishMethod = buildboxcommon::buildboxcommonmetrics::
            StatsDPublisherOptions::PublishMethod::StdErr;

    std::string publishPath = "";
    int publishPort = 0;

    if (RECC_METRICS_UDP_SERVER.size()) {
        publishMethod = buildboxcommon::buildboxcommonmetrics::
            StatsDPublisherOptions::PublishMethod::UDP;
        Env::parse_host_port_string(RECC_METRICS_UDP_SERVER, publishPath,
                                    &publishPort);
    }
    else if (RECC_METRICS_FILE.size()) {
        publishMethod = buildboxcommon::buildboxcommonmetrics::
            StatsDPublisherOptions::PublishMethod::File;
        publishPath = RECC_METRICS_FILE;
    }

    return StatsDPublisherType(publishMethod, publishPath, publishPort);
}

} // namespace recc
} // namespace BloombergLP
