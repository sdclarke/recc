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

#ifndef INCLUDED_LOGGING
#define INCLUDED_LOGGING

#include <cerrno>
#include <cstdio>
#include <env.h>
#include <iostream>

#ifdef RECC_DEBUG
#define RECC_LOG_VERBOSE(x)                                                   \
    if (RECC_VERBOSE) {                                                       \
        std::clog << __FILE__ << ":" << __PRETTY_FUNCTION__ << ":"            \
                  << __LINE__ << " ";                                         \
        std::clog << x << "\n";                                               \
    }
#else
#define RECC_LOG_VERBOSE(x)                                                   \
    if (RECC_VERBOSE) {                                                       \
        std::clog << x << "\n";                                               \
    }

#endif

#define RECC_LOG_PERROR(x) perror(x);

#define RECC_LOG(x) std::cout << x << std::endl;

#define RECC_LOG_ERROR(x) std::cerr << x << "\n";

#define RECC_LOG_WARNING(x) std::cerr << x << "\n";

#define RECC_LOG_DEBUG(x)                                                     \
    std::clog << __FILE__ << ":" << __LINE__ << ":" << x << "\n";

using BloombergLP::recc::RECC_VERBOSE;
#endif
