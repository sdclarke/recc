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

#include <digestgenerator.h>

#include <string>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using proto::Digest;

TEST(CASHashTest, EmptyString)
{
    const auto digest = DigestGenerator::make_digest("");

    EXPECT_EQ(
        digest.hash(),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    EXPECT_EQ(digest.size_bytes(), 0);
}

TEST(DigestGeneratorTest, String)
{
    using namespace std::literals::string_literals;
    const std::string testString(
        "This is a sample blob to hash. \0 It contains some NUL characters "
        "\0."s);

    const auto digest = DigestGenerator::make_digest(testString);

    const std::string expected_sha256_hash =
        "b1c4daf6e3812505064c07f1ad0b1d6693d93b1b28c452e55ad17e38c30e89aa";

    EXPECT_EQ(digest.hash(), expected_sha256_hash);
    EXPECT_EQ(digest.size_bytes(), testString.size());
}

TEST(DigestGeneratorTest, Proto)
{
    // Creating an arbitrary proto:
    proto::Command commandProto;
    *commandProto.add_arguments() = "command";
    *commandProto.add_arguments() = "-v";
    *commandProto.add_arguments() = "-1";
    *commandProto.add_output_files() = "output.txt";

    // Creating a digest from it:
    const Digest protoDigest = DigestGenerator::make_digest(commandProto);

    // And creating a digest from its serialization:
    const std::string serializedProto = commandProto.SerializeAsString();
    const Digest stringDigest = DigestGenerator::make_digest(serializedProto);

    EXPECT_EQ(protoDigest, stringDigest);
}
