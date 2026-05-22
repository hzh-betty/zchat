#include "common/proto_mapper.h"

#include <cstdint>

namespace zchat {

zchat::UserInfo ToProtoUser(const UserRecord &user,
                            const std::string &avatar_content) {
    zchat::UserInfo proto;
    proto.set_user_id(user.user_id);
    proto.set_nickname(user.nickname);
    proto.set_description(user.description);
    proto.set_phone(user.phone);
    proto.set_avatar(avatar_content);
    return proto;
}

zchat::MessageInfo ToProtoMessage(const MessageRecord &message,
                                  const UserRecord &sender,
                                  const std::string &file_content) {
    zchat::MessageInfo proto;
    proto.set_message_id(message.message_id);
    proto.set_chat_session_id(message.session_id);
    proto.set_timestamp(message.create_time);
    *proto.mutable_sender() = ToProtoUser(sender, "");
    auto *content = proto.mutable_message();
    content->set_message_type(
        static_cast<zchat::MessageType>(message.message_type));
    switch (content->message_type()) {
    case zchat::STRING:
        content->mutable_string_message()->set_content(message.content);
        break;
    case zchat::IMAGE:
        content->mutable_image_message()->set_file_id(message.file_id);
        content->mutable_image_message()->set_image_content(file_content);
        break;
    case zchat::FILE:
        content->mutable_file_message()->set_file_id(message.file_id);
        content->mutable_file_message()->set_file_name(message.file_name);
        content->mutable_file_message()->set_file_size(
            static_cast<std::int64_t>(message.file_size));
        content->mutable_file_message()->set_file_contents(file_content);
        break;
    case zchat::SPEECH:
        content->mutable_speech_message()->set_file_id(message.file_id);
        content->mutable_speech_message()->set_file_contents(file_content);
        break;
    default:
        content->set_message_type(zchat::STRING);
        content->mutable_string_message()->set_content(message.content);
        break;
    }
    return proto;
}

MessageRecord ToMessageRecord(const zchat::NewMessageReq &request,
                              const std::string &message_id,
                              const std::string &user_id,
                              std::int64_t create_time,
                              std::string *file_content) {
    MessageRecord record;
    record.message_id = message_id;
    record.session_id = request.chat_session_id();
    record.user_id = user_id;
    record.message_type = static_cast<int>(request.message().message_type());
    record.create_time = create_time;

    if (request.message().message_type() == zchat::STRING) {
        record.content = request.message().string_message().content();
    } else if (request.message().message_type() == zchat::IMAGE) {
        record.file_id = message_id;
        if (request.message().image_message().has_file_id() &&
            !request.message().image_message().file_id().empty()) {
            record.file_id = request.message().image_message().file_id();
        }
        if (request.message().image_message().has_image_content()) {
            *file_content = request.message().image_message().image_content();
        }
    } else if (request.message().message_type() == zchat::FILE) {
        record.file_id = message_id;
        if (request.message().file_message().has_file_id() &&
            !request.message().file_message().file_id().empty()) {
            record.file_id = request.message().file_message().file_id();
        }
        if (request.message().file_message().has_file_name()) {
            record.file_name = request.message().file_message().file_name();
        }
        if (request.message().file_message().has_file_size()) {
            record.file_size = static_cast<std::uint64_t>(
                request.message().file_message().file_size());
        }
        if (request.message().file_message().has_file_contents()) {
            *file_content = request.message().file_message().file_contents();
            record.file_size = file_content->size();
        }
    } else if (request.message().message_type() == zchat::SPEECH) {
        record.file_id = message_id;
        if (request.message().speech_message().has_file_id() &&
            !request.message().speech_message().file_id().empty()) {
            record.file_id = request.message().speech_message().file_id();
        }
        if (request.message().speech_message().has_file_contents()) {
            *file_content = request.message().speech_message().file_contents();
            record.file_size = file_content->size();
        }
    }
    return record;
}

} // namespace zchat
