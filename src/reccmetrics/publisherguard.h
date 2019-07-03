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

#ifndef INCLUDED_RECCMETRICS_PUBLISHERGUARD_H
#define INCLUDED_RECCMETRICS_PUBLISHERGUARD_H

namespace BloombergLP {
namespace recc {
namespace reccmetrics {
/**
 * PublisherGuard is the Publisher Guard class
 *
 * It invokes publish() on the Publisher provided when the guard goes out of
 * scope.
 */
template <class PublisherType> class PublisherGuard {
  private:
    PublisherType d_publisher;
    const bool d_enabled;

  public:
    explicit PublisherGuard(bool enabled) : d_publisher(), d_enabled(enabled)
    {
    }

    explicit PublisherGuard(bool enabled, const PublisherType &publisher)
        : d_publisher(publisher), d_enabled(enabled)
    {
    }

    // Destructor
    ~PublisherGuard()
    {
        if (!d_enabled)
            return;
        d_publisher.publish();
    };
};

} // namespace reccmetrics
} // namespace recc
} // namespace BloombergLP
#endif
