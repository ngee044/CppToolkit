#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>

namespace GameNetwork
{
    // Forward declarations
    class GameSession;
    class GameConnection;
    
    // Disconnection reasons
    enum class DisconnectReason : uint8_t
    {
        Unknown = 0,
        Timeout = 1,
        ClientDisconnect = 2,
        ServerShutdown = 3,
        KickedByAdmin = 4,
        NetworkError = 5,
        ProtocolError = 6,
        AuthenticationFailed = 7,
        DuplicateLogin = 8,
        MaintenanceMode = 9,
        InvalidPacket = 10,
        Banned = 11,
        IdleTimeout = 12,
        MaxReconnectAttemptsReached = 13
    };
    
    // Reconnection configuration
    struct ReconnectionConfig
    {
        uint32_t max_reconnect_attempts = 5;
        std::chrono::seconds initial_retry_delay{1};
        std::chrono::seconds max_retry_delay{60};
        float retry_delay_multiplier = 2.0f;
        std::chrono::seconds reconnect_timeout{300}; // 5 minutes
        bool enable_exponential_backoff = true;
    };
    
    // Disconnection event data
    struct DisconnectionEvent
    {
        uint64_t session_id;
        DisconnectReason reason;
        std::chrono::system_clock::time_point timestamp;
        std::string error_message;
        std::optional<std::string> remote_address;
    };
    
    // Reconnection state
    struct ReconnectionState
    {
        uint32_t attempt_count = 0;
        std::chrono::system_clock::time_point last_attempt;
        std::chrono::seconds current_delay{1};
        bool is_reconnecting = false;
        std::string session_token;
        std::optional<std::string> last_error;
    };
    
    // Disconnection handler callbacks
    using OnDisconnectCallback = std::function<void(const DisconnectionEvent&)>;
    using OnReconnectAttemptCallback = std::function<void(uint64_t session_id, uint32_t attempt)>;
    using OnReconnectSuccessCallback = std::function<void(uint64_t session_id)>;
    using OnReconnectFailureCallback = std::function<void(uint64_t session_id, const std::string& error)>;
    
    class DisconnectionHandler
    {
    public:
        DisconnectionHandler();
        virtual ~DisconnectionHandler() = default;
        
        // Configuration
        auto set_config(const ReconnectionConfig& config) -> void;
        auto get_config() const -> ReconnectionConfig;
        
        // Session management
        auto handle_disconnect(uint64_t session_id, DisconnectReason reason, const std::string& error_message = "") 
            -> std::tuple<bool, std::optional<std::string>>;
        auto handle_connection_lost(std::shared_ptr<GameConnection> connection) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Reconnection management
        auto attempt_reconnect(uint64_t session_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto cancel_reconnect(uint64_t session_id) -> void;
        auto is_reconnecting(uint64_t session_id) const -> bool;
        auto get_reconnection_state(uint64_t session_id) const -> std::optional<ReconnectionState>;
        
        // Session preservation
        auto preserve_session_data(uint64_t session_id, std::shared_ptr<GameSession> session) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto restore_session_data(uint64_t session_id) 
            -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>;
        auto cleanup_preserved_session(uint64_t session_id) -> void;
        
        // Error recovery
        auto should_attempt_reconnect(DisconnectReason reason) const -> bool;
        auto calculate_next_retry_delay(const ReconnectionState& state) const -> std::chrono::seconds;
        
        // Statistics and monitoring
        auto get_disconnection_count() const -> uint64_t;
        auto get_reconnection_success_rate() const -> float;
        auto get_average_reconnection_time() const -> std::chrono::milliseconds;
        auto get_recent_disconnections(size_t count = 10) const -> std::vector<DisconnectionEvent>;
        
        // Event callbacks
        auto set_on_disconnect_callback(OnDisconnectCallback callback) -> void;
        auto set_on_reconnect_attempt_callback(OnReconnectAttemptCallback callback) -> void;
        auto set_on_reconnect_success_callback(OnReconnectSuccessCallback callback) -> void;
        auto set_on_reconnect_failure_callback(OnReconnectFailureCallback callback) -> void;        
        // Cleanup and maintenance
        auto cleanup_expired_sessions() -> void;
        auto reset_statistics() -> void;
        
    private:
        // Internal state
        ReconnectionConfig config_;
        std::unordered_map<uint64_t, ReconnectionState> reconnection_states_;
        std::unordered_map<uint64_t, std::shared_ptr<GameSession>> preserved_sessions_;
        std::vector<DisconnectionEvent> recent_disconnections_;
        
        // Statistics
        std::atomic<uint64_t> total_disconnections_{0};
        std::atomic<uint64_t> successful_reconnections_{0};
        std::atomic<uint64_t> failed_reconnections_{0};
        std::chrono::milliseconds total_reconnection_time_{0};
        
        // Callbacks
        OnDisconnectCallback on_disconnect_;
        OnReconnectAttemptCallback on_reconnect_attempt_;
        OnReconnectSuccessCallback on_reconnect_success_;
        OnReconnectFailureCallback on_reconnect_failure_;
        
        // Helper methods
        auto notify_disconnect(const DisconnectionEvent& event) -> void;
        auto notify_reconnect_attempt(uint64_t session_id, uint32_t attempt) -> void;
        auto notify_reconnect_success(uint64_t session_id) -> void;
        auto notify_reconnect_failure(uint64_t session_id, const std::string& error) -> void;
        
        auto create_reconnection_state(uint64_t session_id) -> ReconnectionState&;
        auto remove_reconnection_state(uint64_t session_id) -> void;
        auto update_statistics(uint64_t session_id, bool success, std::chrono::milliseconds duration) -> void;
    };
}