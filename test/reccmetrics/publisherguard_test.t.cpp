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
#include <reccmetrics/publisherguard.h>

class MockPublisherThrowsOnPublish {
  public:
    void publish() { throw std::runtime_error("publish called"); }
};

bool PUBLISHER_PUBLISHED;
class MockPublisherUpdatesGlobalOnPublish {
  public:
    void publish()
    {
        // Make sure this is the first call to publish()
        ASSERT_EQ(false, PUBLISHER_PUBLISHED);
        // Update global
        PUBLISHER_PUBLISHED = true;
    }
};

using namespace BloombergLP::recc::reccmetrics;

TEST(ReccmetricsTest, PublisherGuardTestDisabled)
{
    {
        PublisherGuard<MockPublisherThrowsOnPublish> publisherGuard(false);
    }
}

TEST(ReccmetricsTest, PublisherGuardTestEnabled)
{
    /*
      Note: Ideally this would've been detected in a more elegant way
            using ASSERT_THROW(), couldn't get it work though.
            If you figure it out, please fix this test!!
     */
    PUBLISHER_PUBLISHED = false;
    ASSERT_EQ(false, PUBLISHER_PUBLISHED);
    {
        ASSERT_EQ(false, PUBLISHER_PUBLISHED);
        {
            PublisherGuard<MockPublisherUpdatesGlobalOnPublish> publisherGuard(
                true);
            ASSERT_EQ(false, PUBLISHER_PUBLISHED);
        }
        ASSERT_EQ(true, PUBLISHER_PUBLISHED);
    }
    ASSERT_EQ(true, PUBLISHER_PUBLISHED);
}
