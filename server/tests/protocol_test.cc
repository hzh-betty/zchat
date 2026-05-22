#include <cassert>
#include <string>
#include <type_traits>

#include "common/config.h"
#include "common/domain_records.h"
#include "common/noncopyable.h"
#include "common/protobuf_http.h"
#include "common/result.h"
#include "common/uuid.h"
#include "message/message_search_index.h"
#include "transmite/message_queue.h"
#include "user.pb.h"

int main() {
    static_assert(!std::is_copy_constructible_v<zchat::NonCopyable>);
    static_assert(!std::is_copy_assignable_v<zchat::NonCopyable>);

    auto void_ok = zchat::Result<void>::Ok();
    assert(void_ok.ok());
    auto void_fail = zchat::Result<void>::Fail("void error");
    assert(!void_fail.ok());
    assert(void_fail.error().message == "void error");

    auto value_ok = zchat::Result<std::string>::Ok("variant result");
    assert(value_ok.ok());
    assert(value_ok.value() == "variant result");
    auto value_fail = zchat::Result<std::string>::Fail("value error");
    assert(!value_fail.ok());
    assert(value_fail.error().message == "value error");

    zchat::UserLoginRsp response;
    response.set_request_id(zchat::NewRequestId());
    response.set_success(true);
    response.set_errmsg("");
    response.set_login_session_id("session");

    const auto http_response = zchat::ProtobufResponse(response);
    assert(http_response->statusCode() == drogon::k200OK);

    zchat::UserLoginRsp parsed;
    assert(parsed.ParseFromString(std::string(http_response->body())));
    assert(parsed.success());
    assert(parsed.login_session_id() == "session");

    zchat::MessageRecord message;
    message.message_id = "msg-1";
    message.session_id = "session-1";
    message.user_id = "user-1";
    message.message_type = 0;
    message.create_time = 1710000000;
    message.content = "hello \"zchat\"";
    message.file_id = "file-1";
    message.file_name = "note.txt";
    message.file_size = 12;

    const std::string document =
        zchat::BuildElasticsearchMessageDocument(message);
    assert(document.find("\"message_id\":\"msg-1\"") != std::string::npos);
    assert(document.find("\"content\":\"hello \\\"zchat\\\"\"") !=
           std::string::npos);
    assert(document.find("\"file_size\":12") != std::string::npos);

    zchat::RabbitmqConfig rabbitmq;
    rabbitmq.host = "127.0.0.1:5673";
    rabbitmq.user = "guest";
    rabbitmq.password = "guest";
    assert(zchat::BuildRabbitmqAddress(rabbitmq) ==
           "amqp://guest:guest@127.0.0.1:5673/");
    return 0;
}
