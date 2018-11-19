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

#include <env.h>
#include <iostream>
#include <logging.h>
#include <recccounterguard.h>
#include <sstream>

using namespace std;

namespace BloombergLP {
namespace recc {

ReccCounterGuard::ReccCounterGuard(int limit)
{
    if (limit <= 0 && limit != ReccCounterGuard::NO_LIMIT) {
        throw std::invalid_argument("Invalid limit initialization.");
    }
    else {
        this->limit_left = limit;
    }
}

bool ReccCounterGuard::is_unlimited()
{
    return this->limit_left == ReccCounterGuard::NO_LIMIT;
}

bool ReccCounterGuard::is_allowed_more()
{
    return (this->is_unlimited() || this->limit_left > 0);
}

int ReccCounterGuard::get_limit() { return (this->limit_left); }

int ReccCounterGuard::get_limit_from_args(int arg)
{
    int job_limit = ReccCounterGuard::NO_LIMIT;
    if (arg != ReccCounterGuard::NO_LIMIT) {
        if (arg > 0) {
            job_limit = arg;
        }
        else {
            RECC_LOG_WARNING(
                "Warning: RECC_JOBS_COUNT set to an invalid value ("
                << arg << "), defaulting to NO_LIMIT");
        }
    }
    return job_limit;
}

void ReccCounterGuard::decrease_limit()
{
    if (!this->is_unlimited()) {
        if (this->limit_left > 0) {
            this->limit_left -= 1;
            RECC_LOG_VERBOSE(this->limit_left
                             << " left before terminating worker");
        }
        else {
            throw std::out_of_range("The limit was reached already");
        }
    }
}
} // namespace recc
} // namespace BloombergLP
