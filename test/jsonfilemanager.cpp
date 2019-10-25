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

#include <fileutils.h>
#include <jsonfilemanager.h>

#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace testing;

// A Simple class which will create a temporary file
// and destroy the file when the destructor is called.
// Useful for test file write and read capabilities.
class TmpFile {
  public:
    std::string d_fileName;
    TmpFile(const std::string &tmpFileName)
    {
        d_fileName = tmpFileName;
        std::ofstream outfile(tmpFileName);
    }
    ~TmpFile() { remove(d_fileName.c_str()); }
};

// We are using TmpFile here instead of the AuthSessionFiles
// Fixture because the latter has a dependency on JsonFileManager
TEST(JsonFileManagerTest, Read)
{
    const std::string currDir = FileUtils::get_current_working_directory();
    std::string s_clientFilePath = currDir + "/" + "clientToken";

    JsonFileManager reader(s_clientFilePath);
    std::string contents = reader.read();
    std::string expectedContents =
        "{\"access_token\": \"old_fake\", \"refresh_token\": \"old_fake\"}";
    EXPECT_EQ(expectedContents, contents);
}

TEST(JsonFileManagerTest, Write)
{
    const std::string currDir = FileUtils::get_current_working_directory();
    std::string tmpFilePath = currDir + "/" + "FakeTmpFile.fake";

    TmpFile tmpStore(tmpFilePath);
    JsonFileManager writer(tmpFilePath);
    std::string tmpWritten = "THIS FILE SHOULD NOT EXIST! ERROR!";
    writer.write(tmpWritten);
    std::string contents = writer.read();
    EXPECT_EQ(tmpWritten, contents);
}
