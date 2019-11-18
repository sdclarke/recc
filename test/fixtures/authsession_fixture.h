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

#ifndef INCLUDED_AUTHSESSION_FIXTURE
#define INCLUDED_AUTHSESSION_FIXTURE

#include <jsonfilemanager.h>

#include <gtest/gtest.h>

using BloombergLP::recc::JsonFileManager;

class AuthSessionFiles : public ::testing::Test {
  protected:
    /**
     * Global set-up.
     * Called before the first test in the test suite.
     * Initializes path names and files
     */
    static void SetUpTestCase();

    /**
     * Global tear down.
     * Called after last test in the test suite.
     * Closes files which are still open
     */
    static void TearDownTestCase();

    /**
     * Run before every test
     * Resets file contents
     */
    virtual void SetUp();

    /**
     * Run after every test
     * Resets file contents
     */
    virtual void TearDown();

    static JsonFileManager *s_clientFile;
    static std::string s_clientFilePath;

  private:
    static std::string s_clientFileContents;
};

#endif
