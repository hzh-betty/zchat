#ifndef ZCHAT_SERVER_SRC_COMMON_PROTOBUF_HTTP_H_
#define ZCHAT_SERVER_SRC_COMMON_PROTOBUF_HTTP_H_

#include <memory>
#include <string>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <google/protobuf/message.h>

namespace zchat {

std::shared_ptr<drogon::HttpResponse>
ProtobufResponse(const google::protobuf::Message &message);

std::shared_ptr<drogon::HttpResponse> TextResponse(const std::string &body);

bool ParseProtobufRequest(const drogon::HttpRequestPtr &request,
                          google::protobuf::Message *message);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_PROTOBUF_HTTP_H_
