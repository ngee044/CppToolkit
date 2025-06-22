#include "DisconnectionHandler.h"
#include "GameSession.h"
#include "GameConnection.h"
#include <algorithm>
#include <mutex>
#include <thread>

namespace GameNetwork
{
    // Static mutex for thread safety
    static std::mutex disconnection_mutex;
    
    DisconnectionHandler::DisconnectionHandler()
        : config_()
    {
        recent_disconnections_.reserve(1000); // Pre-allocate for performance
    }
    
    auto DisconnectionHandler::set_config(const ReconnectionConfig& config) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        config_ = config;
    }
    
    auto DisconnectionHandler::get_config() const -> ReconnectionConfig
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        return config_;
    }
    
    auto DisconnectionHandler::handle_disconnect(uint64_t session_id, DisconnectReason reason, 
                                                const std::string& error_message) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        
        // Create disconnection event
        DisconnectionEvent event
        {
            session_id,
            reason,
            std::chrono::system_clock::now(),
            error_message,
            std::nullopt // Remote address will be filled by connection handler
        };
        
        // Store in recent disconnections
        recent_disconnections_.push_back(event);
        if (recent_disconnections_.size() > 1000)
        {
            recent_disconnections_.erase(recent_disconnections_.begin());
        }
        
        // Update statistics
        total_disconnections_++;
        
        // Notify listeners
        notify_disconnect(event);
        
        // Check if reconnection should be attempted
        if (should_attempt_reconnect(reason))
        {
            auto& state = create_reconnection_state(session_id);
            state.is_reconnecting = true;
            return {true, std::nullopt};
        }
        
        return {false, "Reconnection not attempted for reason: " + std::to_string(static_cast<int>(reason))};
    }    
    auto DisconnectionHandler::handle_connection_lost(std::shared_ptr<GameConnection> connection) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!connection)
        {
            return {false, "Invalid connection pointer"};
        }
        
        auto session_id = connection->session_id();
        return handle_disconnect(session_id, DisconnectReason::NetworkError, "Connection lost");
    }
    
    auto DisconnectionHandler::attempt_reconnect(uint64_t session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        
        auto it = reconnection_states_.find(session_id);
        if (it == reconnection_states_.end())
        {
            return {false, "No reconnection state found for session"};
        }
        
        auto& state = it->second;
        
        // Check max attempts
        if (state.attempt_count >= config_.max_reconnect_attempts)
        {
            remove_reconnection_state(session_id);
            notify_reconnect_failure(session_id, "Max reconnection attempts reached");
            failed_reconnections_++;
            return {false, "Max reconnection attempts reached"};
        }        
        // Check timeout
        auto now = std::chrono::system_clock::now();
        auto time_since_disconnect = std::chrono::duration_cast<std::chrono::seconds>(
            now - state.last_attempt);
        
        if (time_since_disconnect > config_.reconnect_timeout)
        {
            remove_reconnection_state(session_id);
            notify_reconnect_failure(session_id, "Reconnection timeout exceeded");
            failed_reconnections_++;
            return {false, "Reconnection timeout exceeded"};
        }
        
        // Update state
        state.attempt_count++;
        state.last_attempt = now;
        
        // Calculate delay
        if (state.attempt_count > 1)
        {
            state.current_delay = calculate_next_retry_delay(state);
            
            // Sleep for retry delay
            std::this_thread::sleep_for(state.current_delay);
        }
        
        // Notify attempt
        notify_reconnect_attempt(session_id, state.attempt_count);
        
        return {true, std::nullopt};
    }    
    auto DisconnectionHandler::cancel_reconnect(uint64_t session_id) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        remove_reconnection_state(session_id);
    }
    
    auto DisconnectionHandler::is_reconnecting(uint64_t session_id) const -> bool
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        auto it = reconnection_states_.find(session_id);
        return it != reconnection_states_.end() && it->second.is_reconnecting;
    }
    
    auto DisconnectionHandler::get_reconnection_state(uint64_t session_id) const 
        -> std::optional<ReconnectionState>
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        auto it = reconnection_states_.find(session_id);
        if (it != reconnection_states_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }
    
    auto DisconnectionHandler::preserve_session_data(uint64_t session_id, 
                                                    std::shared_ptr<GameSession> session) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session)
        {
            return {false, "Invalid session pointer"};
        }        
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        preserved_sessions_[session_id] = session;
        return {true, std::nullopt};
    }
    
    auto DisconnectionHandler::restore_session_data(uint64_t session_id) 
        -> std::tuple<std::shared_ptr<GameSession>, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        auto it = preserved_sessions_.find(session_id);
        if (it != preserved_sessions_.end())
        {
            auto session = it->second;
            preserved_sessions_.erase(it);
            return {session, std::nullopt};
        }
        return {nullptr, "No preserved session found"};
    }
    
    auto DisconnectionHandler::cleanup_preserved_session(uint64_t session_id) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        preserved_sessions_.erase(session_id);
    }
    
    auto DisconnectionHandler::should_attempt_reconnect(DisconnectReason reason) const -> bool
    {
        switch (reason)
        {
            case DisconnectReason::Timeout:
            case DisconnectReason::NetworkError:
            case DisconnectReason::ProtocolError:
                return true;                
            case DisconnectReason::ClientDisconnect:
            case DisconnectReason::ServerShutdown:
            case DisconnectReason::KickedByAdmin:
            case DisconnectReason::AuthenticationFailed:
            case DisconnectReason::DuplicateLogin:
            case DisconnectReason::MaintenanceMode:
            case DisconnectReason::Banned:
            case DisconnectReason::MaxReconnectAttemptsReached:
                return false;
                
            case DisconnectReason::Unknown:
            case DisconnectReason::InvalidPacket:
            case DisconnectReason::IdleTimeout:
            default:
                return true; // Attempt reconnect for unknown/ambiguous cases
        }
    }
    
    auto DisconnectionHandler::calculate_next_retry_delay(const ReconnectionState& state) const 
        -> std::chrono::seconds
    {
        if (!config_.enable_exponential_backoff)
        {
            return config_.initial_retry_delay;
        }
        
        auto next_delay = std::chrono::duration_cast<std::chrono::seconds>(
            state.current_delay * config_.retry_delay_multiplier);
        
        return std::min(next_delay, config_.max_retry_delay);
    }    
    auto DisconnectionHandler::get_disconnection_count() const -> uint64_t
    {
        return total_disconnections_.load();
    }
    
    auto DisconnectionHandler::get_reconnection_success_rate() const -> float
    {
        auto success = successful_reconnections_.load();
        auto failed = failed_reconnections_.load();
        auto total = success + failed;
        
        if (total == 0)
        {
            return 0.0f;
        }
        
        return static_cast<float>(success) / static_cast<float>(total);
    }
    
    auto DisconnectionHandler::get_average_reconnection_time() const -> std::chrono::milliseconds
    {
        auto success = successful_reconnections_.load();
        if (success == 0)
        {
            return std::chrono::milliseconds{0};
        }
        
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        return total_reconnection_time_ / success;
    }    
    auto DisconnectionHandler::get_recent_disconnections(size_t count) const 
        -> std::vector<DisconnectionEvent>
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        
        if (recent_disconnections_.size() <= count)
        {
            return recent_disconnections_;
        }
        
        std::vector<DisconnectionEvent> result;
        result.reserve(count);
        
        auto start = recent_disconnections_.end() - count;
        std::copy(start, recent_disconnections_.end(), std::back_inserter(result));
        
        return result;
    }
    
    auto DisconnectionHandler::set_on_disconnect_callback(OnDisconnectCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        on_disconnect_ = callback;
    }
    
    auto DisconnectionHandler::set_on_reconnect_attempt_callback(OnReconnectAttemptCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        on_reconnect_attempt_ = callback;
    }    
    auto DisconnectionHandler::set_on_reconnect_success_callback(OnReconnectSuccessCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        on_reconnect_success_ = callback;
    }
    
    auto DisconnectionHandler::set_on_reconnect_failure_callback(OnReconnectFailureCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        on_reconnect_failure_ = callback;
    }
    
    auto DisconnectionHandler::cleanup_expired_sessions() -> void
    {
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        
        auto now = std::chrono::system_clock::now();
        
        // Clean up reconnection states
        for (auto it = reconnection_states_.begin(); it != reconnection_states_.end();)
        {
            auto time_since_last_attempt = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_attempt);
            
            if (time_since_last_attempt > config_.reconnect_timeout)
            {
                it = reconnection_states_.erase(it);
            }
            else
            {
                ++it;
            }
        }        
        // Clean up preserved sessions older than timeout
        for (auto it = preserved_sessions_.begin(); it != preserved_sessions_.end();)
        {
            // Check if session has expired (implementation depends on GameSession)
            // For now, we'll just check if the pointer is still valid
            if (!it->second)
            {
                it = preserved_sessions_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    
    auto DisconnectionHandler::reset_statistics() -> void
    {
        total_disconnections_ = 0;
        successful_reconnections_ = 0;
        failed_reconnections_ = 0;
        
        std::lock_guard<std::mutex> lock(disconnection_mutex);
        total_reconnection_time_ = std::chrono::milliseconds{0};
        recent_disconnections_.clear();
    }
    
    // Private helper methods
    auto DisconnectionHandler::notify_disconnect(const DisconnectionEvent& event) -> void
    {
        if (on_disconnect_)
        {
            on_disconnect_(event);
        }
    }    
    auto DisconnectionHandler::notify_reconnect_attempt(uint64_t session_id, uint32_t attempt) -> void
    {
        if (on_reconnect_attempt_)
        {
            on_reconnect_attempt_(session_id, attempt);
        }
    }
    
    auto DisconnectionHandler::notify_reconnect_success(uint64_t session_id) -> void
    {
        if (on_reconnect_success_)
        {
            on_reconnect_success_(session_id);
        }
    }
    
    auto DisconnectionHandler::notify_reconnect_failure(uint64_t session_id, const std::string& error) -> void
    {
        if (on_reconnect_failure_)
        {
            on_reconnect_failure_(session_id, error);
        }
    }
    
    auto DisconnectionHandler::create_reconnection_state(uint64_t session_id) -> ReconnectionState&
    {
        auto [it, inserted] = reconnection_states_.emplace(session_id, ReconnectionState{});
        if (!inserted)
        {
            // Reset existing state
            it->second = ReconnectionState{};
        }
        it->second.last_attempt = std::chrono::system_clock::now();
        return it->second;
    }    
    auto DisconnectionHandler::remove_reconnection_state(uint64_t session_id) -> void
    {
        reconnection_states_.erase(session_id);
    }
    
    auto DisconnectionHandler::update_statistics(uint64_t session_id, bool success, 
                                                std::chrono::milliseconds duration) -> void
    {
        if (success)
        {
            successful_reconnections_++;
            total_reconnection_time_ += duration;
            notify_reconnect_success(session_id);
        }
        else
        {
            failed_reconnections_++;
        }
    }
}