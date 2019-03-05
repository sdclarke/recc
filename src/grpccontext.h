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

#ifndef INCLUDED_GRPCCONTEXT
#define INCLUDED_GRPCCONTEXT

#include <authsession.h>

#include <grpcpp/security/credentials.h>

namespace BloombergLP {
namespace recc {

class GrpcContext {
  public:
    typedef std::unique_ptr<grpc::ClientContext> GrpcClientContextPtr;

    GrpcContext(AuthBase *authSession) : d_authSession(authSession) {}

    GrpcContext() : d_authSession(NULL) {}

    /**
     * Build a new ClientContext object for rpc calls.
     * If an authSession is set, it will set the context's
     * authentication credentials.
     */
    GrpcClientContextPtr new_client_context();

    /**
     * Refresh AuthSession. If AuthSession not set, this will
     * throw a runtime error.
     */
    void auth_refresh();

    /**
     * Set a new AuthSession
     */
    void set_auth(AuthBase *authSession);

  private:
    AuthBase *d_authSession;
};

} // namespace recc
} // namespace BloombergLP

#endif
