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

#ifndef INCLUDED_FORMPOST
#define INCLUDED_FORMPOST

#include <string>

namespace BloombergLP {
namespace recc {

class Post {
  public:
    virtual std::string generate_post(std::string refresh_token) = 0;
};

class FormPost : public Post {
  public:
    std::string generate_post(std::string refresh_token);
};

} // namespace recc
} // namespace BloombergLP

#endif