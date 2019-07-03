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
//

#include <authsession.h>
#include <env.h>
#include <fileutils.h>
#include <formpost.h>
#include <jsonfilemanager.h>
#include <logging.h>
#include <protos.h>

#include <curl/curl.h>
#include <fstream>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/util/json_util.h>
#include <sstream>
#include <stdio.h>
#include <string>

namespace BloombergLP {
namespace recc {

namespace {

// Function which libcurl calls to write server response to string
size_t write_server_response(char *contents, size_t size, size_t nmemb,
                             std::ostringstream *store)
{
    store->write(contents, nmemb);
    return size * nmemb;
}

} // Unnamed namespace

AuthBase::~AuthBase() {}
AuthSession::AuthSession(Post *formPostFactory)
{
    init_jwt();
    d_formPostFactory = formPostFactory;
    if (RECC_AUTH_REFRESH_URL != "") {
        CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
        // throw an error on failure
        const std::string error_case = curl_easy_strerror(res);
        if (res != CURLE_OK) {
            throw std::runtime_error("curl_global_init() failed: " +
                                     error_case);
        }
    }
}

AuthSession::~AuthSession()
{
    if (RECC_AUTH_REFRESH_URL != "") {
        curl_global_cleanup();
    }
}

std::string AuthSession::get_access_token()
{
    return d_jwtToken.access_token();
}

void AuthSession::refresh_current_token()
{
    // Just re-read token if URL not specified
    if (RECC_AUTH_REFRESH_URL == "") {
        RECC_LOG_VERBOSE("Refreshing Token: reloading token from file");
        init_jwt();
    }
    else {
        RECC_LOG_VERBOSE("Refreshing Token: sending curl request to server");
        const std::string formPost =
            d_formPostFactory->generate_post(d_jwtToken.refresh_token());
        const std::string response = post_form(formPost);
        d_jwtToken = construct_token(response, "From Auth Server", true);
        JsonFileManager jwtFile(RECC_JWT_JSON_FILE_PATH);
        jwtFile.write(response);
    }
}

std::string AuthSession::post_form(const std::string &formPost)
{
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    CURLcode res;

    std::ostringstream response;
    if (curl) {
        // set the headers
        headers = curl_slist_append(
            headers, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Post Request
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formPost.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);

        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, RECC_AUTH_REFRESH_URL.c_str());

        // Only allow HTTPS, FILE and SFTP protocols.
        // HTTP is not included, to prevent refresh tokens leaking
        // on a insecure connection. FILE is used for testing.
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS,
                         CURLPROTO_HTTPS | CURLPROTO_FILE | CURLPROTO_SFTP);

        // set function to read server response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_server_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            const std::string error_code = curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            throw std::runtime_error("curl_easy_perform() failed: " +
                                     error_code);
        }

        // Avoid memory leaks
        curl_easy_cleanup(curl);
    }
    else {
        throw std::runtime_error("curl_easy_init() failed: returned NULL ptr");
    }

    return response.str();
}

proto::AccessTokenResponse
AuthSession::construct_token(const std::string &jsonString,
                             const std::string &jsonPath, bool refresh)
{
    // Using protobuf's internal json->protobuf utility to avoid json parser
    // dependencies. Util converts the json to a protobuf message
    // which can then be read.
    google::protobuf::StringPiece jsonSp(jsonString);
    proto::AccessTokenResponse reccAuth;
    google::protobuf::util::JsonParseOptions options;
    // Won't return error in case json has fields not declared in
    // proto::AccessTokenResponse. Don't want code to break if metadata fields
    // are added
    options.ignore_unknown_fields = true;
    const google::protobuf::util::Status status =
        google::protobuf::util::JsonStringToMessage(jsonSp, &reccAuth,
                                                    options);
    if (!status.ok()) {
        RECC_LOG_ERROR(jsonString);
        const std::string errMsg =
            JwtErrorUtil::error_to_string(jsonPath, JwtError::BadFormat);
        throw std::runtime_error(errMsg);
    }
    const grpc::string access_token = reccAuth.access_token();
    if (access_token.empty()) {
        RECC_LOG_ERROR(jsonString);
        const std::string errMsg = JwtErrorUtil::error_to_string(
            jsonPath, JwtError::MissingAccessTokenField);
        throw std::runtime_error(errMsg);
    }

    const grpc::string refresh_token = reccAuth.refresh_token();
    if (refresh_token.empty() && refresh) {
        RECC_LOG_ERROR(jsonString);
        const std::string errMsg =
            JwtErrorUtil::error_to_string(jsonPath, MissingRefreshTokenField);
        throw std::runtime_error(errMsg);
    }

    return reccAuth;
}

void AuthSession::init_jwt()
{
    JsonFileManager jwtFile(RECC_JWT_JSON_FILE_PATH);
    const std::string jsonString = jwtFile.read();
    d_jwtToken = construct_token(jsonString, RECC_JWT_JSON_FILE_PATH);
}

std::string JwtErrorUtil::error_to_string(const std::string &jsonPath,
                                          const JwtError &type)
{
    std::ostringstream errMsg;
    errMsg << "JWT authentication token " << jsonPath;
    switch (type) {
        case JwtError::NotExist:
            errMsg << " can't be read";
            break;
        case JwtError::BadFormat:
            errMsg << " could not be parsed as JSON";
            break;
        case JwtError::MissingAccessTokenField:
            errMsg << " missing field access_token";
            break;
        case JwtError::MissingRefreshTokenField:
            errMsg << " missing field refresh_token";
            break;
    }
    return errMsg.str();
}

} // namespace recc
} // namespace BloombergLP
