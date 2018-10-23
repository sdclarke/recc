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

#ifndef INCLUDED_RECCDEFAULTS
#define INCLUDED_RECCDEFAULTS

#include <chrono>

// clang-format off

#define DEFAULT_RECC_WORKER_POLL_WAIT std::chrono::milliseconds(250)
#define DEFAULT_RECC_POLL_WAIT std::chrono::seconds(1)
#define DEFAULT_RECC_SERVER "localhost:8085"
#define DEFAULT_RECC_INSTANCE "main"
#define DEFAULT_RECC_TMPDIR "/tmp"
#define DEFAULT_RECC_TMP_PREFIX "rec"
#define DEFAULT_RECC_WORKER_TMP_PREFIX "reccworker"
#define DEFAULT_RECC_SERVER_AUTH_GOOGLEAPI 0
#define DEFAULT_RECC_CONFIG "recc.conf"

#define DEFAULT_RECC_VERBOSE 0
#define DEFAULT_RECC_FORCE_REMOTE 0
#define DEFAULT_RECC_ACTION_UNCACHEABLE 0
#define DEFAULT_RECC_SKIP_CACHE 0
#define DEFAULT_RECC_DONT_SAVE_OUTPUT 0
#define DEFAULT_RECC_MAX_CONCURRENT_JOBS 4

#define DEFAULT_RECC_DEPS_DIRECTORY_OVERRIDE ""
#define DEFAULT_RECC_DEPS_OVERRIDE {}
#define DEFAULT_RECC_OUTPUT_FILES_OVERRIDE {}
#define DEFAULT_RECC_OUTPUT_DIRECTORIES_OVERRIDE {}

#define DEFAULT_RECC_DEPS_ENV {}
#define DEFAULT_RECC_REMOTE_ENV {}
#define DEFAULT_RECC_REMOTE_PLATFORM {}

#define DEFAULT_RECC_CONFIG_LOCATIONS {}

#endif
