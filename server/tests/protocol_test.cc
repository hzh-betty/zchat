#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include "common/config.h"
#include "common/domain_records.h"
#include "common/noncopyable.h"
#include "common/proto_mapper.h"
#include "common/protobuf_http.h"
#include "common/result.h"
#include "common/runtime.h"
#include "common/uuid.h"
#include "file/file_repository.h"
#include "friend/friend_repository.h"
#include "message/message_repository.h"
#include "message/message_search_index.h"
#include "message/message_service.h"
#include "transmite/message_queue.h"
#include "user/user_repository.h"
#include "user/user_search_index.h"
#include "user.pb.h"

namespace {

class FakeMessageRepository final : public zchat::MessageRepository {
  public:
    zchat::VoidResult InsertMessage(const zchat::MessageRecord &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::Result<std::vector<zchat::MessageRecord>>
    ListRecentMessages(const std::string &session_id, int) override {
        zchat::MessageRecord message;
        message.message_id = "msg-1";
        message.session_id = session_id;
        message.user_id = "member-1";
        message.message_type = 0;
        message.content = "secret";
        return zchat::Result<std::vector<zchat::MessageRecord>>::Ok({message});
    }
    zchat::Result<std::vector<zchat::MessageRecord>>
    ListMessagesByTime(const std::string &session_id, std::int64_t,
                       std::int64_t) override {
        return ListRecentMessages(session_id, 1);
    }
    zchat::Result<std::vector<zchat::MessageRecord>>
    SearchMessages(const std::string &session_id,
                   const std::string &) override {
        return ListRecentMessages(session_id, 1);
    }
    zchat::Result<std::optional<zchat::MessageRecord>>
    LastMessage(const std::string &session_id) override {
        return zchat::Result<std::optional<zchat::MessageRecord>>::Ok(
            ListRecentMessages(session_id, 1).value().front());
    }
};

class FakeUserRepository final : public zchat::UserRepository {
  public:
    zchat::Result<std::optional<zchat::UserRecord>>
    FindUserById(const std::string &user_id) override {
        zchat::UserRecord user;
        user.user_id = user_id;
        user.nickname = user_id;
        return zchat::Result<std::optional<zchat::UserRecord>>::Ok(user);
    }
    zchat::Result<std::optional<zchat::UserRecord>>
    FindUserByNickname(const std::string &) override {
        return zchat::Result<std::optional<zchat::UserRecord>>::Ok(
            std::nullopt);
    }
    zchat::Result<std::optional<zchat::UserRecord>>
    FindUserByPhone(const std::string &) override {
        return zchat::Result<std::optional<zchat::UserRecord>>::Ok(
            std::nullopt);
    }
    zchat::Result<std::vector<zchat::UserRecord>>
    FindUsersByIds(const std::vector<std::string> &user_ids) override {
        std::vector<zchat::UserRecord> users;
        for (const auto &id : user_ids) {
            zchat::UserRecord user;
            user.user_id = id;
            user.nickname = id;
            users.push_back(user);
        }
        return zchat::Result<std::vector<zchat::UserRecord>>::Ok(users);
    }
    zchat::Result<std::vector<zchat::UserRecord>>
    SearchUsers(const std::string &, const std::string &) override {
        return zchat::Result<std::vector<zchat::UserRecord>>::Ok({});
    }
    zchat::VoidResult InsertUser(const zchat::UserRecord &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult UpdateUserNickname(const std::string &,
                                         const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult UpdateUserDescription(const std::string &,
                                            const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult UpdateUserPhone(const std::string &,
                                      const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult UpdateUserAvatar(const std::string &,
                                       const std::string &) override {
        return zchat::VoidResult::Ok();
    }
};

class FakeFileRepository final : public zchat::FileRepository {
  public:
    zchat::VoidResult PutFile(const zchat::FileRecord &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::Result<std::optional<zchat::FileRecord>>
    GetFile(const std::string &) override {
        return zchat::Result<std::optional<zchat::FileRecord>>::Ok(
            std::nullopt);
    }
};

class FakeFriendRepository final : public zchat::FriendRepository {
  public:
    zchat::VoidResult InsertRelation(const std::string &,
                                     const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult DeleteRelation(const std::string &,
                                     const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::Result<bool> RelationExists(const std::string &,
                                       const std::string &) override {
        return zchat::Result<bool>::Ok(false);
    }
    zchat::Result<std::vector<std::string>>
    ListFriendIds(const std::string &) override {
        return zchat::Result<std::vector<std::string>>::Ok({});
    }
    zchat::VoidResult
    InsertFriendApply(const zchat::FriendApplyRecord &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult DeleteFriendApply(const std::string &,
                                        const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::Result<bool> FriendApplyExists(const std::string &,
                                          const std::string &) override {
        return zchat::Result<bool>::Ok(false);
    }
    zchat::Result<std::vector<zchat::FriendApplyRecord>>
    ListPendingApplies(const std::string &) override {
        return zchat::Result<std::vector<zchat::FriendApplyRecord>>::Ok({});
    }
    zchat::VoidResult
    InsertChatSession(const zchat::ChatSessionRecord &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult InsertChatSessionMember(const std::string &,
                                              const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::VoidResult DeleteSingleChatSession(const std::string &,
                                              const std::string &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::Result<std::vector<zchat::ChatSessionRecord>>
    ListChatSessions(const std::string &) override {
        return zchat::Result<std::vector<zchat::ChatSessionRecord>>::Ok({});
    }
    zchat::Result<std::vector<std::string>>
    ListChatSessionMembers(const std::string &) override {
        return zchat::Result<std::vector<std::string>>::Ok({"member-1"});
    }
    zchat::Result<std::optional<std::string>>
    FindSingleChatPeer(const std::string &, const std::string &) override {
        return zchat::Result<std::optional<std::string>>::Ok(std::nullopt);
    }
};

class DisabledMessageSearchIndex final : public zchat::MessageSearchIndex {
  public:
    zchat::VoidResult IndexMessage(const zchat::MessageRecord &) override {
        return zchat::VoidResult::Ok();
    }
    zchat::Result<std::vector<zchat::MessageRecord>>
    SearchMessages(const std::string &, const std::string &) override {
        return zchat::Result<std::vector<zchat::MessageRecord>>::Ok({});
    }
};

} // namespace

int main() {
    setenv("ZCHAT_EMPTY_ETCD_USERNAME", "", 1);
    setenv("ZCHAT_EMPTY_ETCD_PASSWORD", "", 1);
    setenv("ZCHAT_EMPTY_ES_USER", "", 1);
    setenv("ZCHAT_EMPTY_ES_PASSWORD", "", 1);

    const std::string temp_config_path = "/tmp/zchat_protocol_config_test.json";
    {
        std::ofstream config_file(temp_config_path);
        config_file << R"({
  "etcd": {
    "username": "{{ZCHAT_EMPTY_ETCD_USERNAME}}",
    "password": "{{ZCHAT_EMPTY_ETCD_PASSWORD}}"
  },
  "elasticsearch": {
    "user": "{{ZCHAT_EMPTY_ES_USER}}",
    "password": "{{ZCHAT_EMPTY_ES_PASSWORD}}"
  },
  "rabbitmq": {
    "host": "rabbitmq.local",
    "port": 5678,
    "user": "guest",
    "password": "guest"
  }
})";
    }
    const zchat::AppConfig loaded_config = zchat::LoadConfig(temp_config_path);
    assert(loaded_config.etcd.username.empty());
    assert(loaded_config.etcd.password.empty());
    assert(loaded_config.elasticsearch.user.empty());
    assert(loaded_config.elasticsearch.password.empty());
    assert(loaded_config.rabbitmq.host == "rabbitmq.local");
    assert(loaded_config.rabbitmq.port == 5678);

    char program_name[] = "zchat_user_service";
    char *default_args[] = {program_name};
    assert(zchat::ConfigPath(1, default_args, "server/config/user.json") ==
           "server/config/user.json");

    char config_path[] = "server/config/override.json";
    char *override_args[] = {program_name, config_path};
    assert(zchat::ConfigPath(2, override_args, "server/config/user.json") ==
           "server/config/override.json");

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
    const std::string message_index =
        zchat::BuildElasticsearchMessageIndexDefinition();
    assert(message_index.find("\"session_id\":{\"type\":\"keyword\"}") !=
           std::string::npos);
    assert(message_index.find("\"content\":{\"type\":\"text\"}") !=
           std::string::npos);

    zchat::RabbitmqConfig rabbitmq;
    rabbitmq.host = "127.0.0.1";
    rabbitmq.port = 5673;
    rabbitmq.user = "guest";
    rabbitmq.password = "guest";
    assert(zchat::BuildRabbitmqAddress(rabbitmq) ==
           "amqp://guest:guest@127.0.0.1:5673/");

    zchat::UserRecord sender;
    sender.user_id = "user-1";
    sender.nickname = "sender";
    const std::string user_document =
        zchat::BuildElasticsearchUserDocument(sender);
    assert(user_document.find("\"user_id\":\"user-1\"") != std::string::npos);
    const std::string user_search =
        zchat::BuildElasticsearchUserSearchRequest("sender", {"user-1"});
    assert(user_search.find("\"minimum_should_match\":1") !=
           std::string::npos);
    assert(user_search.find("\"user_id.keyword\":[\"user-1\"]") !=
           std::string::npos);
    const std::string user_index =
        zchat::BuildElasticsearchUserIndexDefinition();
    assert(user_index.find("\"phone\":{\"type\":\"keyword\"}") !=
           std::string::npos);
    assert(user_index.find("\"nickname\":{\"type\":\"text\"}") !=
           std::string::npos);

    const zchat::MessageInfo queued =
        zchat::ToProtoMessage(message, sender, "file-body");
    std::string queued_file_content;
    const zchat::MessageRecord parsed_message =
        zchat::FromProtoMessage(queued, &queued_file_content);
    assert(parsed_message.message_id == message.message_id);
    assert(parsed_message.session_id == message.session_id);
    assert(parsed_message.user_id == message.user_id);
    assert(parsed_message.message_type == message.message_type);
    assert(parsed_message.content == message.content);
    assert(queued_file_content.empty());

    FakeMessageRepository fake_messages;
    FakeUserRepository fake_users;
    FakeFileRepository fake_files;
    FakeFriendRepository fake_friends;
    DisabledMessageSearchIndex disabled_search;
    zchat::MessageService message_service(fake_messages, fake_users,
                                          fake_files, fake_friends,
                                          disabled_search);
    zchat::GetRecentMsgReq recent_request;
    recent_request.set_request_id("req-authz");
    recent_request.set_user_id("intruder");
    recent_request.set_chat_session_id("session-1");
    recent_request.set_msg_count(10);
    const auto recent_response = message_service.GetRecent(recent_request);
    assert(!recent_response.success());
    recent_request.set_user_id("member-1");
    const auto allowed_recent_response =
        message_service.GetRecent(recent_request);
    assert(allowed_recent_response.success());
    assert(allowed_recent_response.msg_list_size() == 1);
    return 0;
}
