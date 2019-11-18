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

#include <authsession.h>
#include <authsession_fixture.h>
#include <env.h>
#include <formpost.h>
#include <grpccontext.h>

#include <gmock/gmock.h>
#include <grpcpp/security/credentials.h>
#include <gtest/gtest.h>

using namespace BloombergLP::recc;
using namespace testing;

class MockAuthSession : public AuthBase {
  public:
    MOCK_METHOD0(get_access_token, std::string());
    MOCK_METHOD0(refresh_current_token, void());
};
