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

#ifndef INCLUDED_AUTHSESSION
#define INCLUDED_AUTHSESSION

#include <formpost.h>
#include <protos.h>

#include <string>

namespace BloombergLP {
namespace recc {

/**
 * Different json error cases
 */
enum JwtError {
    NotExist,
    BadFormat,
    MissingAccessTokenField,
    MissingRefreshTokenField
};

class JwtErrorUtil {
  public:
    /**
     * Returns the appropriate error string for a given json error case.
     */
    static std::string error_to_string(const std::string &jsonPath,
                                       const JwtError &type);
};

/**
 * Abstract class interface which GrpcContext will use
 */
class AuthBase {
  public:
    virtual std::string get_access_token() = 0;
    virtual void refresh_current_token() = 0;
};

/**
 * A class which loads and manages
 * the authentication token
 */
class AuthSession : public AuthBase {
  public:
    /**
     * Constructor creates a JsonFileManager object around
     * RECC_JWT_JSON_FILE_PATH. It will also create a
     * proto::AccessTokenResponse object from the file path. It will initialize
     * curl if RECC_AUTH_REFRESH_URL is not set to NULL
     */
    AuthSession(Post *formPostFactory);

    /**
     * Clean up curl if RECC_AUTH_REFRESH_URL is set
     */
    ~AuthSession();

    /**
     * Will retrieve the access_token from d_jwtToken.
     * Doesn't do any checks since token should be valid
     */
    std::string get_access_token();

    /**
     * Will attemp to refresh jwt token and write new token into
     * RECC_JWT_JSON_FILE_PATH, if RECC_AUTH_REFRESH_URL is set.
     * Will only write in new token if it is a valid jwt object. Will throw
     * runtime error otherwise. Also sets d_jwtToken with new token.
     * If RECC_AUTH_REFRESH_URL is not set, it will reset d_jwtToken using the
     * token in  RECC_JWT_JSON_FILE_PATH.
     */
    void refresh_current_token();

  private:
    /**
     * a protobuf object representing a jwt token.
     * Do not construct/alter this object directly. Use
     * construct_token if need be.
     */
    proto::AccessTokenResponse d_jwtToken;

    /**
     * A class which will generate a properly formatted post request
     */
    Post *d_formPostFactory;

    /**
     * Use libcurl to make a post request to
     * RECC_AUTH_REFRESH_URL. Returns server response.
     */
    std::string post_form(const std::string &formPost);

    /**
     * verify if string is a proper json jwt object.
     * If so, will return a protobuf object which represents the jwt object.
     * Will throw runtime error if not.
     */
    proto::AccessTokenResponse construct_token(const std::string &jsonString,
                                               const std::string &jsonPath,
                                               bool refresh = false);

    void init_jwt();
};

} // namespace recc
} // namespace BloombergLP

#endif
