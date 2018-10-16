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

#include <iostream>
#include <pthread.h>
#include <remote_execution_signals.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

namespace BloombergLP {
namespace recc {

void setup_signal_handler(int signal, void (*handler)(int))
{
    /* Set up signal handling for the Execute() request */
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(signal, &sa, NULL) == -1) {
        cerr << "Warning: unable to register cancellation handler" << endl;
    }
}

void block_sigint()
{
    sigset_t signal_set;
    sigaddset(&signal_set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
}

void unblock_sigint()
{
    sigset_t signal_set;
    sigaddset(&signal_set, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, &signal_set, NULL);
}

} // namespace recc
} // namespace BloombergLP
