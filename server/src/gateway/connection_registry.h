#ifndef ZCHAT_SERVER_SRC_GATEWAY_CONNECTION_REGISTRY_H_
#define ZCHAT_SERVER_SRC_GATEWAY_CONNECTION_REGISTRY_H_

#include "common/noncopyable.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include <drogon/WebSocketConnection.h>

#include "common/logger.h"

namespace zchat {

class ConnectionRegistry : public NonCopyable {
  public:
    ConnectionRegistry() = default;

    ~ConnectionRegistry() = default;

    void Bind(const std::string &user_id, const std::string &session_id,
              const drogon::WebSocketConnectionPtr &connection) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string key = ConnectionKey(connection);
        users_by_connection_[key] = user_id;
        sessions_by_connection_[key] = session_id;
        connections_by_user_[user_id] = connection;
        ZCHAT_LOG_DEBUG("bind websocket user={} session={} connection={}",
                        user_id, session_id, key);
    }

    bool Remove(const drogon::WebSocketConnectionPtr &connection,
                std::string *user_id, std::string *session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string key = ConnectionKey(connection);
        bool removed_current_connection = false;
        const auto user_it = users_by_connection_.find(key);
        if (user_it != users_by_connection_.end()) {
            if (user_id != nullptr) {
                *user_id = user_it->second;
            }
            const auto current = connections_by_user_.find(user_it->second);
            if (current != connections_by_user_.end() &&
                current->second.lock().get() == connection.get()) {
                connections_by_user_.erase(current);
                removed_current_connection = true;
            }
            ZCHAT_LOG_DEBUG("remove websocket user={} connection={}",
                            user_it->second, key);
            users_by_connection_.erase(user_it);
        }
        const auto session_it = sessions_by_connection_.find(key);
        if (session_it != sessions_by_connection_.end()) {
            if (session_id != nullptr) {
                *session_id = session_it->second;
            }
            sessions_by_connection_.erase(session_it);
        }
        return removed_current_connection;
    }

    void SendToUser(const std::string &user_id, const std::string &payload) {
        drogon::WebSocketConnectionPtr connection;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = connections_by_user_.find(user_id);
            if (it == connections_by_user_.end()) {
                ZCHAT_LOG_DEBUG("skip notify user={} reason=no-connection",
                                user_id);
                return;
            }
            connection = it->second.lock();
        }
        if (connection != nullptr) {
            connection->send(payload, drogon::WebSocketMessageType::Binary);
            ZCHAT_LOG_DEBUG("sent websocket notify user={} size={}B", user_id,
                            payload.size());
        } else {
            ZCHAT_LOG_DEBUG("skip notify user={} reason=expired-connection",
                            user_id);
        }
    }

    void ForEachBoundUser(const std::function<void(const std::string &)> &fn) {
        std::vector<std::string> user_ids;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            user_ids.reserve(connections_by_user_.size());
            for (const auto &[user_id, conn] : connections_by_user_) {
                if (!conn.expired()) {
                    user_ids.push_back(user_id);
                }
            }
        }
        for (const auto &user_id : user_ids) {
            fn(user_id);
        }
    }

    bool IsBound(const drogon::WebSocketConnectionPtr &connection) {
        std::lock_guard<std::mutex> lock(mutex_);
        return users_by_connection_.find(ConnectionKey(connection)) !=
               users_by_connection_.end();
    }

  private:
    static std::string
    ConnectionKey(const drogon::WebSocketConnectionPtr &connection) {
        return std::to_string(
            reinterpret_cast<std::uintptr_t>(connection.get()));
    }

    std::mutex mutex_;
    std::unordered_map<std::string, std::weak_ptr<drogon::WebSocketConnection>>
        connections_by_user_;
    std::unordered_map<std::string, std::string> users_by_connection_;
    std::unordered_map<std::string, std::string> sessions_by_connection_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_CONNECTION_REGISTRY_H_
