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

#include <env.h>
#include <fileutils.h>
#include <grpcchannels.h>
#include <protos.h>

#include <fstream>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/util/json_util.h>
#include <grpcpp/create_channel.h>
#include <sstream>
#include <string>

namespace BloombergLP {
namespace recc {

namespace {
std::shared_ptr<grpc::CallCredentials> get_call_creds()
{
    grpc::string access_token =
        GrpcChannels::get_jwt_token(expand_path(RECC_JWT_JSON_FILE_PATH));
    std::shared_ptr<grpc::CallCredentials> call_creds =
        grpc::AccessTokenCredentials(access_token);
    return call_creds;
}

std::shared_ptr<grpc::ChannelCredentials> get_channel_creds()
{
    return grpc::SslCredentials(grpc::SslCredentialsOptions());
}

} // Unnamed namespace

GrpcChannels GrpcChannels::get_channels_from_config()
{
    if (RECC_SERVER_AUTH_GOOGLEAPI && (RECC_SERVER_SSL || RECC_SERVER_JWT)) {
        throw std::invalid_argument(
            "RECC_SERVER_AUTH_GOOGLEAPI & "
            "RECC_SERVER_SSL/RECC_SERVER_JWT cannot both be set.");
    }
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (RECC_SERVER_AUTH_GOOGLEAPI) {
        creds = grpc::GoogleDefaultCredentials();
    }
    else if (RECC_SERVER_JWT) {
        creds = grpc::CompositeChannelCredentials(get_channel_creds(),
                                                  get_call_creds());
    }
    else if (RECC_SERVER_SSL) {
        creds = get_channel_creds();
    }
    else {
        creds = grpc::InsecureChannelCredentials();
    }
    return GrpcChannels(grpc::CreateChannel(RECC_SERVER, creds),
                        grpc::CreateChannel(RECC_CAS_SERVER, creds));
}

grpc::string GrpcChannels::get_jwt_token(const std::string &json_path)
{
    std::ifstream json_file(json_path);
    if (!json_file.good()) {
        const std::string err_msg = jwt_error(json_path, jwtError::NotExist);
        throw std::runtime_error(err_msg);
    }
    std::ostringstream buffer;
    buffer << json_file.rdbuf();
    std::string json_string = buffer.str();

    // Using protobuf's internal json->protobuf utility to avoid json parser
    // dependencies. Util converts the json to a protobuf message
    // which can then be read.
    google::protobuf::StringPiece json_sp(json_string);
    proto::AccessTokenResponse recc_auth;
    google::protobuf::util::JsonParseOptions options;
    // Won't return error in case json has fields not declared in
    // proto::AccessTokenResponse. Don't want code to break if metadata fields
    // are added
    options.ignore_unknown_fields = true;
    google::protobuf::util::Status status;
    status = google::protobuf::util::JsonStringToMessage(json_sp, &recc_auth,
                                                         options);
    if (!status.ok()) {
        std::string err_msg = jwt_error(json_path, jwtError::BadFormat);
        throw std::runtime_error(err_msg);
    }
    const grpc::string access_token = recc_auth.access_token();
    if (access_token == "") {
        std::string err_msg =
            jwt_error(json_path, jwtError::MissingAccessTokenField);
        throw std::runtime_error(err_msg);
    }

    return access_token;
}

std::string GrpcChannels::jwt_error(const std::string &json_path,
                                    const jwtError &type)
{
    std::ostringstream err_msg;
    err_msg << "JWT authentication token: " << json_path;
    switch (type) {
        case NotExist:
            err_msg << " does not exist or can't be read";
            break;
        case BadFormat:
            err_msg << " found but JSON could not be parsed";
            break;
        case MissingAccessTokenField:
            err_msg << " missing field access_token";
            break;
    }
    return err_msg.str();
}

} // namespace recc
} // namespace BloombergLP
