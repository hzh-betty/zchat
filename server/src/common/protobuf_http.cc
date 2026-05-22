#include "common/protobuf_http.h"

#include <utility>

#include <drogon/HttpRequest.h>

namespace zchat {

std::shared_ptr<drogon::HttpResponse>
ProtobufResponse(const google::protobuf::Message &message) {
    auto response = drogon::HttpResponse::newHttpResponse();
    std::string body;
    message.SerializeToString(&body);
    response->setBody(std::move(body));
    response->setStatusCode(drogon::k200OK);
    response->addHeader("Content-Type", "application/x-protobuf");
    return response;
}

std::shared_ptr<drogon::HttpResponse> TextResponse(const std::string &body) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setBody(body);
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeCode(drogon::CT_TEXT_PLAIN);
    return response;
}

bool ParseProtobufRequest(const drogon::HttpRequestPtr &request,
                          google::protobuf::Message *message) {
    if (request == nullptr || message == nullptr) {
        return false;
    }
    const std::string body(request->body());
    return message->ParseFromString(body);
}

} // namespace zchat
