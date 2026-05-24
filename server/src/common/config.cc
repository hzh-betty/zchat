#include "common/config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <json/json.h>

namespace zchat {
namespace {

Json::Value ReadJsonFile(const std::string &path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return Json::Value(Json::objectValue);
    }

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        return Json::Value(Json::objectValue);
    }
    return root;
}

std::string ResolveEnv(const std::string &value) {
    if (value.size() >= 4 && value.front() == '{' && value[1] == '{' &&
        value[value.size() - 2] == '}' && value[value.size() - 1] == '}') {
        std::string env_name = value.substr(2, value.size() - 4);
        const char *env_value = std::getenv(env_name.c_str());
        if (env_value != nullptr) {
            return std::string(env_value);
        }
        return std::string();
    }
    return value;
}

std::string GetString(const Json::Value &value, const char *key,
                      const std::string &fallback) {
    if (!value.isObject() || !value.isMember(key)) {
        return fallback;
    }
    return ResolveEnv(value[key].asString());
}

int GetInt(const Json::Value &value, const char *key, int fallback) {
    if (!value.isObject() || !value.isMember(key)) {
        return fallback;
    }
    return value[key].asInt();
}

std::size_t GetSize(const Json::Value &value, const char *key,
                    std::size_t fallback) {
    if (!value.isObject() || !value.isMember(key)) {
        return fallback;
    }
    return static_cast<std::size_t>(value[key].asUInt64());
}

bool GetBool(const Json::Value &value, const char *key, bool fallback) {
    if (!value.isObject() || !value.isMember(key)) {
        return fallback;
    }
    return value[key].asBool();
}

} // namespace

AppConfig LoadConfig(const std::string &path) {
    AppConfig config;
    const Json::Value root = ReadJsonFile(path);

    const Json::Value server = root["server"];
    config.server.http_port =
        GetInt(server, "http_port", config.server.http_port);
    config.server.websocket_port =
        GetInt(server, "websocket_port", config.server.websocket_port);

    const Json::Value services = root["services"];
    config.services.user = GetInt(services, "user", config.services.user);
    config.services.file = GetInt(services, "file", config.services.file);
    config.services.speech = GetInt(services, "speech", config.services.speech);
    config.services.transmite =
        GetInt(services, "transmite", config.services.transmite);
    config.services.message =
        GetInt(services, "message", config.services.message);
    config.services.friend_service =
        GetInt(services, "friend", config.services.friend_service);

    const Json::Value mysql = root["mysql"];
    config.mysql.host = GetString(mysql, "host", config.mysql.host);
    config.mysql.port = GetInt(mysql, "port", config.mysql.port);
    config.mysql.database = GetString(mysql, "database", config.mysql.database);
    config.mysql.user = GetString(mysql, "user", config.mysql.user);
    config.mysql.password = GetString(mysql, "password", config.mysql.password);
    config.mysql.charset = GetString(mysql, "charset", config.mysql.charset);
    config.mysql.connections =
        GetSize(mysql, "connections", config.mysql.connections);

    const Json::Value redis = root["redis"];
    config.redis.host = GetString(redis, "host", config.redis.host);
    config.redis.port = GetInt(redis, "port", config.redis.port);
    config.redis.database =
        static_cast<unsigned int>(GetInt(redis, "database", 0));
    config.redis.password = GetString(redis, "password", config.redis.password);
    config.redis.connections =
        GetSize(redis, "connections", config.redis.connections);

    const Json::Value storage = root["storage"];
    config.storage.path = GetString(storage, "path", config.storage.path);

    const Json::Value speech = root["speech"];
    config.speech.enabled = GetBool(speech, "enabled", config.speech.enabled);
    config.speech.app_id = GetString(speech, "app_id", config.speech.app_id);
    config.speech.api_key = GetString(speech, "api_key", config.speech.api_key);
    config.speech.secret_key =
        GetString(speech, "secret_key", config.speech.secret_key);
    config.speech.placeholder_result = GetString(
        speech, "placeholder_result", config.speech.placeholder_result);

    const Json::Value elasticsearch = root["elasticsearch"];
    config.elasticsearch.enabled =
        GetBool(elasticsearch, "enabled", config.elasticsearch.enabled);
    config.elasticsearch.hosts =
        GetString(elasticsearch, "hosts", config.elasticsearch.hosts);
    config.elasticsearch.user =
        GetString(elasticsearch, "user", config.elasticsearch.user);
    config.elasticsearch.password =
        GetString(elasticsearch, "password", config.elasticsearch.password);

    const Json::Value rabbitmq = root["rabbitmq"];
    config.rabbitmq.enabled =
        GetBool(rabbitmq, "enabled", config.rabbitmq.enabled);
    config.rabbitmq.host = GetString(rabbitmq, "host", config.rabbitmq.host);
    config.rabbitmq.user = GetString(rabbitmq, "user", config.rabbitmq.user);
    config.rabbitmq.password =
        GetString(rabbitmq, "password", config.rabbitmq.password);
    config.rabbitmq.exchange =
        GetString(rabbitmq, "exchange", config.rabbitmq.exchange);
    config.rabbitmq.queue = GetString(rabbitmq, "queue", config.rabbitmq.queue);
    config.rabbitmq.routing_key =
        GetString(rabbitmq, "routing_key", config.rabbitmq.routing_key);

    const Json::Value sms = root["sms"];
    config.sms.enabled = GetBool(sms, "enabled", config.sms.enabled);
    config.sms.access_key_id =
        GetString(sms, "access_key_id", config.sms.access_key_id);
    config.sms.access_key_secret =
        GetString(sms, "access_key_secret", config.sms.access_key_secret);
    config.sms.region = GetString(sms, "region", config.sms.region);
    config.sms.sign_name = GetString(sms, "sign_name", config.sms.sign_name);
    config.sms.template_code =
        GetString(sms, "template_code", config.sms.template_code);
    return config;
}

std::string BuildMysqlConnectionString(const MysqlConfig &config) {
    std::ostringstream stream;
    stream << "host=" << config.host << " port=" << config.port
           << " dbname=" << config.database << " user=" << config.user
           << " password=" << config.password
           << " client_encoding=" << config.charset;
    return stream.str();
}

} // namespace zchat
