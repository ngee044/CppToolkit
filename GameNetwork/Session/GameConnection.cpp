#include "GameConnection.h"
#include "../Packet/GamePacket.h"
#include "../Packet/PacketProcessor.h"

#include <chrono>

namespace GameNetwork
{
    GameConnection::GameConnection(const std::string& connection_id)
        : connection_id_(connection_id)
        , state_(ConnectionState::Disconnected)
        , last_activity_(std::chrono::steady_clock::now())
        , bytes_sent_(0)
        , bytes_received_(0)
        , packets_sent_(0)
        , packets_received_(0)
    {
    }
    
    GameConnection::~GameConnection()
    {
        detach_network_session();
    }
    
    auto GameConnection::attach_network_session(std::shared_ptr<Network::NetworkSession> session) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (network_session_)
        {
            network_session_.reset();
        }
        
        network_session_ = session;
        setup_network_callbacks();
        set_state(ConnectionState::Connected);
        update_last_activity();
    }
    
    auto GameConnection::detach_network_session() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (network_session_)
        {
            network_session_.reset();
            set_state(ConnectionState::Disconnected);
        }
    }
    
    auto GameConnection::is_connected() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return network_session_ != nullptr && state_ >= ConnectionState::Connected;
    }
    
    auto GameConnection::connection_id() const -> std::string
    {
        return connection_id_;
    }
    
    auto GameConnection::account_id() const -> std::string
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return account_id_;
    }
    
    auto GameConnection::set_account_id(const std::string& account_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        account_id_ = account_id;
    }    
    auto GameConnection::state() const -> ConnectionState
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }
    
    auto GameConnection::set_state(ConnectionState new_state) -> void
    {
        state_ = new_state;
    }
    
    auto GameConnection::authenticate(const std::string& account_id, const std::string& session_token) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ConnectionState::Connected)
        {
            return {false, "Connection not in valid state for authentication"};
        }
        
        set_state(ConnectionState::Authenticating);
        
        // TODO: Implement actual authentication logic
        // For now, just store the credentials
        account_id_ = account_id;
        session_token_ = session_token;
        
        set_state(ConnectionState::Authenticated);
        update_last_activity();
        
        return {true, std::nullopt};
    }    
    auto GameConnection::is_authenticated() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ >= ConnectionState::Authenticated;
    }
    
    auto GameConnection::send_packet(const GamePacket& packet, PacketPriority priority) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!network_session_)
        {
            return {false, "No network session attached"};
        }
        
        // TODO: Serialize packet properly
        auto data = packet.serialize();
        
        // Send as binary data through network session
        auto result = network_session_->send_binary(data, "game_packet");
        
        if (std::get<0>(result))
        {
            packets_sent_++;
            bytes_sent_ += data.size();
            update_last_activity();
        }
        
        return result;
    }    
    auto GameConnection::register_packet_handler(std::function<void(const GamePacket&)> handler) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        packet_handler_ = handler;
    }
    
    auto GameConnection::update_last_activity() -> void
    {
        last_activity_ = std::chrono::steady_clock::now();
    }
    
    auto GameConnection::last_activity_time() const -> std::chrono::steady_clock::time_point
    {
        return last_activity_;
    }
    
    auto GameConnection::is_timeout() const -> bool
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_);
        return elapsed > SESSION_TIMEOUT;
    }
    
    auto GameConnection::bytes_sent() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return bytes_sent_;
    }
    
    auto GameConnection::bytes_received() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return bytes_received_;
    }    
    auto GameConnection::packets_sent() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return packets_sent_;
    }
    
    auto GameConnection::packets_received() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return packets_received_;
    }
    
    auto GameConnection::setup_network_callbacks() -> void
    {
        if (!network_session_)
        {
            return;
        }
        
        // Register binary data callback for game packets
        network_session_->received_binary_callback(
            [this](const std::string& id, const std::string& sub_id, 
                   const std::string& message, const std::vector<uint8_t>& data) 
            {
                return on_network_binary(data);
            });
        
        // Register message callback for text-based messages
        network_session_->received_message_callback(
            [this](const std::string& id, const std::string& sub_id, const std::string& message) 
            {
                return on_network_message(message);
            });
    }    
    auto GameConnection::on_network_message(const std::string& message) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Handle text-based messages if needed
        update_last_activity();
        return {true, std::nullopt};
    }
    
    auto GameConnection::on_network_binary(const std::vector<uint8_t>& data) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        update_last_activity();
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(mutex_);
            bytes_received_ += data.size();
            packets_received_++;
        }
        
        // Deserialize packet
        auto [packet, error] = GamePacket::deserialize(data);
        if (!packet)
        {
            return {false, error};
        }
        
        // Call registered handler
        if (packet_handler_)
        {
            packet_handler_(*packet);
        }
        
        return {true, std::nullopt};
    }
}