#include "InterServerCommunication.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <boost/json.hpp>
#include <boost/json/error.hpp>
#include <boost/system/error_code.hpp>

using namespace Utilities;

namespace GameNetwork
{
    InterServerCommunication::InterServerCommunication(const std::string& server_id, ServerType type)
        : server_id_(server_id)
        , server_type_(type)
        , current_load_(0)
        , max_capacity_(1000)
        , next_correlation_id_(1)
        , heartbeat_enabled_(false)
        , heartbeat_interval_(std::chrono::seconds(30))
        , is_running_(false)
    {
        stats_ = {};
    }
    
    InterServerCommunication::~InterServerCommunication()
    {
        stop();
    }
    
    auto InterServerCommunication::start(uint16_t listen_port) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (is_running_)
        {
            return {false, "Server is already running"};
        }
        
        listen_port_ = listen_port;
        
        server_ = std::make_shared<Network::NetworkServer>(server_id_);
        
        server_->received_connection_callback([this](const std::string& id, const std::string& sub_id, const bool& is_connected) -> std::tuple<bool, std::optional<std::string>> {
            if (is_connected)
            {
                Logger::handle().write(LogTypes::Information, fmt::format("Server connected: {}", id));
                auto it = known_servers_.find(id);
                if (it != known_servers_.end())
                {
                    if (!on_server_connected_callbacks_.empty())
                    {
                        for(const auto& cb : on_server_connected_callbacks_)
                        {
                            cb(it->second);
                        }
                    }
                }
            }
            else
            {
                Logger::handle().write(LogTypes::Information, fmt::format("Server disconnected: {}", id));
                auto it = known_servers_.find(id);
                if (it != known_servers_.end())
                {
                    it->second.is_online = false;
                    if (!on_server_disconnected_callbacks_.empty())
                    {
                        for(const auto& cb : on_server_disconnected_callbacks_)
                        {
                            cb(it->second);
                        }
                    }
                }
            }
            return { true, std::nullopt };
        });
        
        server_->received_binary_callback([this](const std::string& id, const std::string& sub_id, const std::string& message, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>> {
            handle_incoming_message(id, message, data);
            return { true, std::nullopt };
        });
        
        auto result = server_->start(listen_port, 8192);
        if (!std::get<0>(result))
        {
            return result;
        }
        
        is_running_ = true;
        return {true, std::nullopt};
    }
    
    auto InterServerCommunication::stop() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_running_)
        {
            return;
        }
        
        is_running_ = false;
        heartbeat_enabled_ = false;
        
        if (heartbeat_thread_.valid())
        {
            heartbeat_thread_.wait();
        }
        
        unregister_from_cluster();
        
        for (auto& [id, connection] : connections_)
        {
            if (connection.client)
            {
                connection.client->stop();
            }
        }
        connections_.clear();
        
        if (server_)
        {
            server_->stop();
            server_.reset();
        }
    }
    
    auto InterServerCommunication::is_running() const -> bool
    {
        return is_running_;
    }
    
    auto InterServerCommunication::register_with_cluster(const std::string& cluster_address, uint16_t cluster_port) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        cluster_address_ = cluster_address;
        cluster_port_ = cluster_port;
        
        ServerInfo self_info;
        self_info.server_id = server_id_;
        self_info.server_name = server_id_;
        self_info.type = server_type_;
        self_info.current_load = current_load_;
        self_info.max_capacity = max_capacity_;
        self_info.is_online = true;
        self_info.last_heartbeat = std::chrono::steady_clock::now();
        
        auto client = std::make_shared<Network::NetworkClient>(server_id_ + "_cluster_client");
        if (!client->start(cluster_address, cluster_port, 8192))
        {
            return {false, "Failed to connect to cluster"};
        }
        
        boost::json::object registration;
        registration["message_type"] = "server_register";
        registration["server_id"] = self_info.server_id;
        registration["server_name"] = self_info.server_name;
        registration["server_type"] = static_cast<int>(self_info.type);
        registration["current_load"] = self_info.current_load;
        registration["max_capacity"] = self_info.max_capacity;
        registration["listen_port"] = listen_port_;
        
        std::string message = boost::json::serialize(registration);
        
        auto send_result = client->send_message(message);
        if (!std::get<0>(send_result))
        {
            return {false, fmt::format("Failed to send registration: {}", std::get<1>(send_result).value_or("Unknown error"))};
        }
        
        cluster_connection_ = client;
        
        known_servers_[server_id_] = self_info;
        
        return {true, std::nullopt};
    }
    
    auto InterServerCommunication::unregister_from_cluster() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!cluster_connection_)
        {
            return;
        }
        
        boost::json::object unregistration;
        unregistration["message_type"] = "server_unregister";
        unregistration["server_id"] = server_id_;
        
        std::string message = boost::json::serialize(unregistration);
        
        cluster_connection_->send_message(message);
        cluster_connection_->stop();
        cluster_connection_.reset();
    }
    
    auto InterServerCommunication::discover_servers(ServerType type) const -> std::vector<ServerInfo>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ServerInfo> result;
        for (const auto& [id, info] : known_servers_)
        {
            if (info.type == type && info.is_online)
            {
                result.push_back(info);
            }
        }
        
        return result;
    }
    
    auto InterServerCommunication::find_server(const std::string& server_id) const -> std::optional<ServerInfo>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = known_servers_.find(server_id);
        if (it != known_servers_.end() && it->second.is_online)
        {
            return it->second;
        }
        
        return std::nullopt;
    }
    
    auto InterServerCommunication::get_least_loaded_server(ServerType type) const -> std::optional<ServerInfo>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::optional<ServerInfo> result;
        float min_load_ratio = 1.0f;
        
        for (const auto& [id, info] : known_servers_)
        {
            if (info.type == type && info.is_online)
            {
                float load_ratio = static_cast<float>(info.current_load) / info.max_capacity;
                if (load_ratio < min_load_ratio)
                {
                    min_load_ratio = load_ratio;
                    result = info;
                }
            }
        }
        
        return result;
    }
    
    auto InterServerCommunication::send_message(const std::string& target_server_id, const std::string& message_type, const std::vector<uint8_t>& payload, bool requires_response)
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto client = get_or_create_connection(target_server_id);
        if (!client)
        {
            return {false, fmt::format("Could not establish connection to {}", target_server_id)};
        }

        boost::json::object msg_json;
        msg_json["message_type"] = message_type;
        msg_json["source_server_id"] = server_id_;
        msg_json["requires_response"] = false;

        std::string message_str = boost::json::serialize(msg_json);
        auto send_result = client->send_binary(payload, message_str);

        if (!std::get<0>(send_result))
        {
            stats_.failed_sends++;
            return {false, fmt::format("Failed to send message: {}", std::get<1>(send_result).value_or("Unknown error"))};
        }

        stats_.messages_sent++;
        stats_.bytes_sent += payload.size() + message_str.length();
        connections_.at(target_server_id).last_active = std::chrono::steady_clock::now();

        return {true, std::nullopt};
    }
    
    auto InterServerCommunication::send_broadcast(ServerType target_type,
                        const std::string& message_type,
                        const std::vector<uint8_t>& payload) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto servers = discover_servers(target_type);
        
        bool all_succeeded = true;
        std::string error_msg;
        
        for (const auto& server : servers)
        {
            if (server.server_id == server_id_) continue;
            auto result = send_message(server.server_id, message_type, payload, false);
            if (!std::get<0>(result))
            {
                all_succeeded = false;
                if (!error_msg.empty()) error_msg += "; ";
                error_msg += "Failed to send to " + server.server_id + ": " + std::get<1>(result).value_or("Unknown error");
            }
        }
        
        if (!all_succeeded)
        {
            return {false, error_msg};
        }
        
        return {true, std::nullopt};
    }

    auto InterServerCommunication::send_request(const std::string& target_server_id, const std::string& message_type, const std::vector<uint8_t>& payload, std::chrono::milliseconds timeout)
        -> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
    {
        auto client = get_or_create_connection(target_server_id);
        if (!client)
        {
            return {std::nullopt, fmt::format("Could not establish connection to {}", target_server_id)};
        }

        uint32_t correlation_id;
        std::future<std::vector<uint8_t>> response_future;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            correlation_id = next_correlation_id_++;
            
            PendingRequest pending;
            pending.correlation_id = correlation_id;
            pending.timeout_time = std::chrono::steady_clock::now() + timeout;
            response_future = pending.response_promise.get_future();
            
            pending_requests_[correlation_id] = std::move(pending);
        }

        boost::json::object msg_json;
        msg_json["message_type"] = message_type;
        msg_json["source_server_id"] = server_id_;
        msg_json["requires_response"] = true;
        msg_json["correlation_id"] = correlation_id;

        std::string message_str = boost::json::serialize(msg_json);
        auto send_result = client->send_binary(payload, message_str);

        if (!std::get<0>(send_result))
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_.erase(correlation_id);
            stats_.failed_sends++;
            return {std::nullopt, fmt::format("Failed to send request: {}", std::get<1>(send_result).value_or("Unknown error"))};
        }

        stats_.messages_sent++;
        stats_.bytes_sent += payload.size() + message_str.length();
        connections_.at(target_server_id).last_active = std::chrono::steady_clock::now();

        auto status = response_future.wait_for(timeout);
        if (status == std::future_status::timeout)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_.erase(correlation_id);
            return {std::nullopt, "Request timed out"};
        }

        try
        {
            return {response_future.get(), std::nullopt};
        }
        catch (const std::future_error& e)
        {
            return {std::nullopt, fmt::format("Future error: {}", e.what())};
        }
    }
    
    auto InterServerCommunication::register_message_handler(const std::string& message_type, MessageHandler handler) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_handlers_[message_type] = handler;
    }
    
    auto InterServerCommunication::unregister_message_handler(const std::string& message_type) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_handlers_.erase(message_type);
    }
    
    auto InterServerCommunication::update_server_load(uint32_t current_load) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_load_ = current_load;
        
        ServerInfo self_info;
        self_info.server_id = server_id_;
        self_info.type = server_type_;
        self_info.current_load = current_load_;
        self_info.max_capacity = max_capacity_;
        
        for (auto& callback : on_server_load_changed_callbacks_)
        {
            callback(self_info);
        }
    }
    
    auto InterServerCommunication::get_server_load() const -> uint32_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_load_;
    }
    
    auto InterServerCommunication::set_max_capacity(uint32_t max_capacity) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        max_capacity_ = max_capacity;
    }
    
    auto InterServerCommunication::enable_heartbeat(std::chrono::seconds interval) -> void
    {
        if(heartbeat_enabled_) return;

        heartbeat_enabled_ = true;
        heartbeat_interval_ = interval;
        
        heartbeat_thread_ = std::async(std::launch::async, [this]() {
            while (heartbeat_enabled_)
            {
                send_heartbeat();
                cleanup_dead_servers();
                std::this_thread::sleep_for(heartbeat_interval_);
            }
        });
    }
    
    auto InterServerCommunication::disable_heartbeat() -> void
    {
        heartbeat_enabled_ = false;
        if (heartbeat_thread_.valid())
        {
            heartbeat_thread_.wait();
        }
    }
    
    auto InterServerCommunication::on_server_connected(ServerEventCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        on_server_connected_callbacks_.push_back(callback);
    }
    
    auto InterServerCommunication::on_server_disconnected(ServerEventCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        on_server_disconnected_callbacks_.push_back(callback);
    }

    auto InterServerCommunication::on_server_load_changed(ServerEventCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        on_server_load_changed_callbacks_.push_back(callback);
    }
    
    auto InterServerCommunication::get_stats() const -> InterServerStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    auto InterServerCommunication::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = {};
    }
    
    auto InterServerCommunication::handle_incoming_message(const std::string& source_server_id, const std::string& message_str, const std::vector<uint8_t>& payload) 
        -> void
    {
        stats_.messages_received++;
        stats_.bytes_received += payload.size() + message_str.length();

        boost::json::value jv;
        boost::system::error_code ec;
        jv = boost::json::parse(message_str, ec);

        if (ec)
        {
            Logger::handle().write(LogTypes::Error, fmt::format("Failed to parse incoming message from {}: {}", source_server_id, ec.message()));
            return;
        }

        try
        {
            boost::json::object const& obj = jv.as_object();
            std::string message_type = boost::json::value_to<std::string>(obj.at("message_type"));

            if (message_type == "heartbeat")
            {
                handle_heartbeat(source_server_id);
            }
            else if (message_type == "response")
            {
                uint32_t correlation_id = boost::json::value_to<uint32_t>(obj.at("correlation_id"));
                handle_response(correlation_id, payload);
            }
            else
            {
                auto it = message_handlers_.find(message_type);
                if (it != message_handlers_.end())
                {
                    InterServerMessage msg;
                    msg.source_server_id = source_server_id;
                    msg.target_server_id = server_id_;
                    msg.message_type = message_type;
                    msg.payload = payload;
                    msg.timestamp = std::chrono::steady_clock::now();
                    msg.requires_response = boost::json::value_to<bool>(obj.at("requires_response"));
                    if (msg.requires_response)
                    {
                        msg.correlation_id = boost::json::value_to<uint32_t>(obj.at("correlation_id"));
                    }

                    auto response_payload = it->second(msg);

                    if (msg.requires_response)
                    {
                        send_response(source_server_id, msg.correlation_id, response_payload);
                    }
                }
                else
                {
                    Logger::handle().write(LogTypes::Error, fmt::format("No handler for message type '{}' from server {}", message_type, source_server_id));
                }
            }
        }
        catch (const std::exception& e)
        {
            Logger::handle().write(LogTypes::Error, fmt::format("Error processing message from {}: {}", source_server_id, e.what()));
        }
    }
    
    auto InterServerCommunication::get_or_create_connection(const std::string& target_server_id) -> std::shared_ptr<Network::NetworkClient>
    {
        std::shared_ptr<Network::NetworkClient> client;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(target_server_id);
            if (it != connections_.end() && it->second.is_connected && it->second.client) 
            {
                return it->second.client;
            }
        }

        auto server_info_opt = find_server(target_server_id);
        if (!server_info_opt) {
            Logger::handle().write(LogTypes::Error, fmt::format("Cannot find server {} to establish connection.", target_server_id));
            return nullptr;
        }

        if (establish_connection(*server_info_opt)) 
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(target_server_id);
            if (it != connections_.end() && it->second.is_connected && it->second.client) 
            {
                return it->second.client;
            }
        }

        return nullptr;
    }

    auto InterServerCommunication::establish_connection(const ServerInfo& server_info) -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connections_.count(server_info.server_id) && connections_.at(server_info.server_id).is_connected) 
        {
            return true;
        }

        Logger::handle().write(LogTypes::Information, fmt::format("Establishing connection to server {} at {}:{}", 
            server_info.server_id, server_info.ip_address, server_info.port));

        auto client = std::make_shared<Network::NetworkClient>(server_id_ + "_to_" + server_info.server_id);

        client->received_connection_callback([this, server_id = server_info.server_id](const bool& is_connected, const bool& by_itself) -> std::tuple<bool, std::optional<std::string>> 
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(server_id);
            if (it != connections_.end()) {
                it->second.is_connected = is_connected;
                if (is_connected) {
                    Logger::handle().write(LogTypes::Information, fmt::format("Connection to {} established.", server_id));
                } else {
                    Logger::handle().write(LogTypes::Error, fmt::format("Connection to {} lost.", server_id));
                }
            }
            return { true, std::nullopt };
        });

        client->received_binary_callback([this, source_id = server_info.server_id](const std::string& msg, const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>> 
        {
            handle_incoming_message(source_id, msg, data);
            return { true, std::nullopt };
        });

        if (!client->start(server_info.ip_address, server_info.port, 8192)) {
            Logger::handle().write(LogTypes::Error, fmt::format("Failed to start connection to {}", server_info.server_id));
            return false;
        }

        Connection conn;
        conn.client = client;
        conn.is_connected = true; // Assume connected, callback will update on failure
        conn.last_active = std::chrono::steady_clock::now();
        connections_[server_info.server_id] = std::move(conn);

        return true;
    }
    
    auto InterServerCommunication::send_heartbeat() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        boost::json::object hb_msg;
        hb_msg["message_type"] = "heartbeat";
        hb_msg["server_id"] = server_id_;
        hb_msg["current_load"] = current_load_;

        std::string message_str = boost::json::serialize(hb_msg);
        std::vector<uint8_t> payload(message_str.begin(), message_str.end());

        for (auto const& [id, conn] : connections_) {
            if (conn.is_connected && conn.client) {
                conn.client->send_binary(payload, message_str);
            }
        }
    }
    
    auto InterServerCommunication::cleanup_dead_servers() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(heartbeat_interval_.count() * 3);

        for (auto it = known_servers_.begin(); it != known_servers_.end(); ) {
            if (it->first == server_id_) {
                ++it;
                continue;
            }
            if (it->second.is_online && (now - it->second.last_heartbeat) > timeout) {
                Logger::handle().write(LogTypes::Error, fmt::format("Server {} timed out. Marking as offline.", it->first));
                it->second.is_online = false;
                for (const auto& cb : on_server_disconnected_callbacks_) {
                    cb(it->second);
                }
            }
            ++it;
        }
    }

    auto InterServerCommunication::handle_heartbeat(const std::string& source_server_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = known_servers_.find(source_server_id);
        if (it != known_servers_.end()) {
            it->second.last_heartbeat = std::chrono::steady_clock::now();
            if (!it->second.is_online) {
                it->second.is_online = true;
                Logger::handle().write(LogTypes::Information, fmt::format("Server {} is back online.", source_server_id));
                for (const auto& cb : on_server_connected_callbacks_) {
                    cb(it->second);
                }
            }
        }
    }

    auto InterServerCommunication::handle_response(uint32_t correlation_id, const std::vector<uint8_t>& payload) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_requests_.find(correlation_id);
        if (it != pending_requests_.end())
        {
            it->second.response_promise.set_value(payload);
            pending_requests_.erase(it);
        }
        else
        {
            Logger::handle().write(LogTypes::Error, fmt::format("Received response for unknown or timed-out correlation ID {}", correlation_id));
        }
    }

    auto InterServerCommunication::send_response(const std::string& target_server_id, uint32_t correlation_id, const std::vector<uint8_t>& payload) -> void
    {
        auto client = get_or_create_connection(target_server_id);
        if (!client) {
            Logger::handle().write(LogTypes::Error, fmt::format("Could not establish connection to {} to send response.", target_server_id));
            return;
        }

        boost::json::object msg_json;
        msg_json["message_type"] = "response";
        msg_json["source_server_id"] = server_id_;
        msg_json["correlation_id"] = correlation_id;

        std::string message_str = boost::json::serialize(msg_json);
        auto send_result = client->send_binary(payload, message_str);

        if (!std::get<0>(send_result))
        {
            stats_.failed_sends++;
            Logger::handle().write(LogTypes::Error, fmt::format("Failed to send response to {}: {}", target_server_id, std::get<1>(send_result).value_or("Unknown error")));
        }
    }
}
