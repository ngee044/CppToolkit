#pragma once

#include "../GameNetworkConstants.h"
#include "GameConnection.h"
#include "../Core/Location.h"

#include <memory>
#include <string>
#include <mutex>
#include <chrono>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <any>
#include <future>
#include <boost/json.hpp>

namespace GameNetwork
{
	class Character;
    
	class GameSession : public std::enable_shared_from_this<GameSession>
	{
	public:
		GameSession(const std::string& session_id, const std::string& account_id);
		virtual ~GameSession();
        
		// Session identification
		auto id() const -> uint64_t;  // Returns hashed session ID
		auto session_id() const -> std::string;
		auto get_session_id() const -> std::string { return session_id(); }  // Alias for compatibility
		auto session_id_hash() const -> uint64_t;
		auto account_id() const -> std::string;
		auto get_account_id() const -> std::string;  // Alias for compatibility
		auto set_account_id(const std::string& id) -> void;
        
		// Connection management
		auto bind_connection(std::shared_ptr<GameConnection> connection) -> void;
		auto unbind_connection() -> void;
		auto current_connection() const -> std::shared_ptr<GameConnection>;
		auto connection() const -> std::shared_ptr<GameConnection>;  // Alias for compatibility
		auto is_online() const -> bool;
		auto is_connected() const -> bool;  // Alias for compatibility
		auto is_active() const -> bool;
        
		// State management
		auto state() const -> SessionConnectionState;
		auto set_state(SessionConnectionState new_state) -> void;
        
		// Character management
		auto load_character(uint64_t character_id) -> std::tuple<bool, std::optional<std::string>>;
		auto current_character() const -> std::shared_ptr<Character>;
		auto save_character() -> std::tuple<bool, std::optional<std::string>>;
        
		// Location management
		auto current_location() const -> const GameNetwork::Location&;
		auto location() const -> const GameNetwork::Location&;  // Alias for compatibility
		auto move_to(const GameNetwork::Location& location) -> void;
		auto teleport_to(const GameNetwork::Location& location) -> void;
        
		// Channel management
		auto enter_channel(uint32_t channel_id) -> std::tuple<bool, std::optional<std::string>>;
		auto leave_channel() -> void;
		auto current_channel_id() const -> uint32_t;
        
		// Session persistence
		auto save_state() -> std::tuple<bool, std::optional<std::string>>;
		auto restore_state() -> std::tuple<bool, std::optional<std::string>>;
        
		// Timeout management
		auto update_last_activity() -> void;
		auto last_activity_time() const -> std::chrono::steady_clock::time_point;
		auto get_last_activity_time() const -> std::chrono::steady_clock::time_point { return last_activity_time(); }  // Alias for compatibility
		auto is_timeout() const -> bool;
		auto remaining_grace_period() const -> std::chrono::seconds;
        
		// Server assignment (for load balancing and migration)
		auto get_current_server_id() const -> std::string;
		auto set_current_server_id(const std::string& server_id) -> void;
		auto get_assigned_server() const -> std::string { return get_current_server_id(); }  // Alias for compatibility
        
		// Session data
		template<typename T>
		auto set_data(const std::string& key, const T& value) -> void;
        
		template<typename T>
		auto get_data(const std::string& key) const -> std::optional<T>;
        
		auto remove_data(const std::string& key) -> void;
        
		// Custom data access methods (for compatibility)
		template<typename T>
		auto set_custom_data(const std::string& key, const T& value) -> void { set_data(key, value); }
        
		template<typename T>
		auto get_custom_data(const std::string& key) const -> std::optional<T> { return get_data<T>(key); }
        
		// Get all custom data (for serialization/migration)
		auto get_all_custom_data() const -> const std::unordered_map<std::string, std::any>&;
        
		// Security token management
		auto set_session_token(const std::string& token) -> void;
		auto get_session_token() const -> std::string;
		auto clear_session_token() -> void;
        
		// Session limits
		auto set_kicked_by_duplicate_login(bool kicked) -> void;
		auto was_kicked_by_duplicate_login() const -> bool;
        
		// Game data management (for migration support)
		auto get_character_id() const -> uint64_t;
		auto set_character_id(uint64_t character_id) -> void;
		auto get_entity_id() const -> uint64_t;
		auto set_entity_id(uint64_t entity_id) -> void;
		auto get_channel_id() const -> uint32_t;
		auto set_channel_id(uint32_t channel_id) -> void;
		auto set_player_location(const GameNetwork::Location& location) -> void;
        
		// Connection status
		auto get_player_location() const -> std::optional<GameNetwork::Location>;
        
		// Packet sending
		auto send_packet(const std::vector<uint8_t>& packet_data) -> bool;
        
		// Additional methods for compatibility
		auto disconnect(const std::string& reason = "") -> void;
		auto set_network_session(std::shared_ptr<GameConnection> connection) -> void;
        
		// ⭐ Session Statistics and Metadata (새로 추가)
		struct SessionStatistics {
			std::chrono::steady_clock::time_point creation_time;
			std::chrono::steady_clock::time_point last_activity_time;
			std::chrono::milliseconds total_session_duration;
			uint64_t packets_sent;
			uint64_t packets_received;
			uint64_t bytes_sent;
			uint64_t bytes_received;
			uint32_t reconnection_count;
			uint32_t command_count;
			std::vector<std::string> activity_log;
		};
        
		auto get_session_statistics() -> SessionStatistics;
		auto set_session_metadata(const std::unordered_map<std::string, std::string>& metadata) -> void;
		auto get_session_metadata() const -> std::unordered_map<std::string, std::string>;
		auto enable_session_recording(bool enable) -> void;
		auto is_session_recording_enabled() const -> bool;
		auto add_activity_log(const std::string& activity) -> void;
		auto clear_activity_log() -> void;
        
	private:
		auto start_grace_period_timer() -> void;
		auto stop_grace_period_timer() -> void;
        
	private:
		mutable std::mutex mutex_;
        
		// Identification
		std::string session_id_;
		std::string account_id_;
        
		// State
		SessionConnectionState state_;
        
		// Connection
		std::shared_ptr<GameConnection> connection_;
        
		// Game data
		std::shared_ptr<Character> character_;
		GameNetwork::Location current_location_;  // Current player location
		uint64_t entity_id_;
		uint64_t character_id_;
        
		// Timing
		std::chrono::steady_clock::time_point created_time_;
		std::chrono::steady_clock::time_point last_activity_;
		std::chrono::steady_clock::time_point disconnected_time_;
        
		// Channel
		uint32_t channel_id_;
        
		// Session data storage
		std::unordered_map<std::string, std::any> custom_data_;
        
		// Server assignment (for load balancing and migration)
		std::string current_server_id_;
        
		// Security
		std::string session_token_;
		bool kicked_by_duplicate_login_;
        
		// Grace period timer
		std::future<void> grace_timer_;
        
		// ⭐ Session Statistics and Metadata (새로 추가)
		std::unordered_map<std::string, std::string> session_metadata_;
		bool session_recording_enabled_;
		uint64_t packets_sent_count_;
		uint64_t packets_received_count_;
		uint64_t bytes_sent_count_;
		uint64_t bytes_received_count_;
		uint32_t reconnection_count_;
		uint32_t command_count_;
		std::vector<std::string> activity_log_;
		static constexpr size_t MAX_ACTIVITY_LOG_SIZE = 1000;
	};

	// Template function implementations
	template<typename T>
	auto GameSession::set_data(const std::string& key, const T& value) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		custom_data_[key] = value;
	}

	template<typename T>
	auto GameSession::get_data(const std::string& key) const -> std::optional<T>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = custom_data_.find(key);
		if (it != custom_data_.end())
		{
			try {
				return std::any_cast<T>(it->second);
			}
			catch (const std::bad_any_cast&) {
				return std::nullopt;
			}
		}
		return std::nullopt;
	}
}
