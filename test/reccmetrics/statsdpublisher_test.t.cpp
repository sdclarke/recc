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
#include <reccmetrics/durationmetrictimer.h>
#include <reccmetrics/metriccollectorfactory.h>
#include <reccmetrics/statsdpublisher.h>
#include <reccmetrics/statsdpublisheroptions.h>
#include <sstream>

using namespace BloombergLP::recc::reccmetrics;

class MockValueType {
  private:
  public:
    static const bool isAggregatable = false;
    MockValueType() {}
    int value() { return -1; }
    const std::string toStatsD(const std::string &name) const { return name; }
};

class AnotherMockValueType1 : public MockValueType {
};

class AnotherMockValueType2 : public MockValueType {
};

TEST(ReccmetricsTest, StatsDPublisherTestRedirectedStdErr)
{
    std::streambuf *actualCerr = std::cerr.rdbuf();
    std::stringstream redirectedCerr;
    // Redirect `cerr` to stringstream
    std::cerr.rdbuf(redirectedCerr.rdbuf());

    // Initialize a StatsDPublisher for MockValueType
    StatsDPublisher<MockValueType> myPublisher(
        StatsDPublisherOptions::PublishMethod::StdErr);

    // Publish
    myPublisher.publish();
    // Expect only title to be printed since there were no actual metrics
    EXPECT_EQ("recc Metrics:\n", redirectedCerr.str());
    // Clear out stringstream
    redirectedCerr.str("");

    // Store "my-metric"
    MetricCollectorFactory::getCollector<MockValueType>()->store(
        "my-metric", MockValueType());
    // Publish
    myPublisher.publish();
    // Expect to have `my-metric` published
    EXPECT_EQ("recc Metrics:\nmy-metric\n", redirectedCerr.str());
    // Clear out stringstream
    redirectedCerr.str("");

    // Store "another-metric"
    MetricCollectorFactory::getCollector<MockValueType>()->store(
        "another-metric", MockValueType());
    // Publish
    myPublisher.publish();
    // Expect to have those two metrics
    EXPECT_NE(std::string::npos, redirectedCerr.str().find("my-metric"));
    EXPECT_NE(std::string::npos, redirectedCerr.str().find("another-metric"));
    // Clear out stringstream
    redirectedCerr.str("");

    // Revert `cerr` redirection
    std::cerr.rdbuf(actualCerr);
}

TEST(ReccmetricsTest, StatsDPublisherTestRedirectedStdErr2ValueTypes)
{
    std::streambuf *actualCerr = std::cerr.rdbuf();
    std::stringstream redirectedCerr;
    // Redirect `cerr` to stringstream
    std::cerr.rdbuf(redirectedCerr.rdbuf());

    // Initialize a StatsDPublisher for MockValueType
    StatsDPublisher<AnotherMockValueType1, AnotherMockValueType2> myPublisher(
        StatsDPublisherOptions::PublishMethod::StdErr);

    // Publish
    myPublisher.publish();
    // Expect only title to be printed since there were no actual metrics
    EXPECT_EQ("recc Metrics:\n", redirectedCerr.str());
    // Clear out stringstream
    redirectedCerr.str("");

    // Store "my-metric"
    MetricCollectorFactory::getCollector<AnotherMockValueType1>()->store(
        "my-metric", AnotherMockValueType1());
    // Publish
    myPublisher.publish();
    // Expect to have `my-metric` published
    EXPECT_EQ("recc Metrics:\nmy-metric\n", redirectedCerr.str());
    // Clear out stringstream
    redirectedCerr.str("");

    // Store "additional-metric"
    MetricCollectorFactory::getCollector<AnotherMockValueType2>()->store(
        "additional-metric", AnotherMockValueType2());
    // Publish
    myPublisher.publish();
    // Expect to have those two metrics
    EXPECT_NE(std::string::npos, redirectedCerr.str().find("my-metric"));
    EXPECT_NE(std::string::npos,
              redirectedCerr.str().find("additional-metric"));
    // Clear out stringstream
    redirectedCerr.str("");

    // Store "another-metric"
    MetricCollectorFactory::getCollector<AnotherMockValueType1>()->store(
        "another-metric", AnotherMockValueType1());
    // Publish
    myPublisher.publish();
    // Expect to have those two metrics
    EXPECT_NE(std::string::npos, redirectedCerr.str().find("my-metric"));
    EXPECT_NE(std::string::npos, redirectedCerr.str().find("another-metric"));
    EXPECT_NE(std::string::npos,
              redirectedCerr.str().find("additional-metric"));
    // Clear out stringstream
    redirectedCerr.str("");

    // Revert `cerr` redirection
    std::cerr.rdbuf(actualCerr);
}
