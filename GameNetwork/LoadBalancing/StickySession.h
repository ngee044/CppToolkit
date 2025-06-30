#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <optional>
#include <functional>

namespace GameNetwork
{
	namespace LoadBalancing
	{
		// Session affinity types
		enum class SessionAffinityType
		{
			None,           // No sticky session
			SourceIP,       // Based on client IP
			Cookie,         // Based on session cookie
			Header,         // Based on custom header
			SessionId       // Based on session ID
		};
        
		// Sticky session configuration
		struct StickySessionConfig
		{
			SessionAffinityType affinity_type{SessionAffinityType::SessionId};
			std::chrono::seconds session_timeout{3600};  // 1 hour default
			std::string cookie_name{"GAMESESSIONID"};
			std::string header_name{"X-Game-Session-Id"};
			bool enable_fallback{true};  // Use another server if sticky server is down
			uint32_t max_sessions_per_server{10000};
		};
        
		// Session mapping information
		struct SessionMapping
		{
			std::string session_id;
			std::string server_id;
			std::string client_identifier;  // IP, cookie value, etc.
			std::chrono::steady_clock::time_point created_time;
			std::chrono::steady_clock::time_point last_access_time;
			uint32_t request_count;
		};
        
		// Sticky session manager
		class StickySessionManager
		{
		public:
			StickySessionManager(const StickySessionConfig& config = {});
			~StickySessionManager();
            
			auto set_config(const StickySessionConfig& config) -> void;
			auto get_config() const -> const StickySessionConfig& { return config_; }
            
			auto get_server_for_session(const std::string& client_identifier, const std::vector<std::string>& available_servers) 
				-> std::optional<std::string>;
            
			auto create_session_mapping(const std::string& client_identifier, const std::string& server_id) 
				-> std::string;
            
			auto touch_session(const std::string& session_id) -> void;
            
			auto remove_session(const std::string& session_id) -> void;
			auto remove_sessions_for_server(const std::string& server_id) -> uint32_t;
            
			auto get_session_count() const -> size_t;
			auto get_session_count_for_server(const std::string& server_id) const -> uint32_t;
			auto get_session_info(const std::string& session_id) const -> std::optional<SessionMapping>;
            
			auto cleanup_expired_sessions() -> uint32_t;
            
			auto export_sessions() const -> std::string; // Export to JSON format
			auto import_sessions(const std::string& json_data) -> std::tuple<bool, std::optional<std::string>>;
            
			using SessionCreatedCallback = std::function<void(const std::string&, const std::string&)>;
			using SessionExpiredCallback = std::function<void(const std::string&)>;
            
			auto on_session_created(SessionCreatedCallback callback) -> void 
			{ session_created_callback_ = callback; }
			auto on_session_expired(SessionExpiredCallback callback) -> void 
			{ session_expired_callback_ = callback; }
            
		private:
			StickySessionConfig config_;
            
			std::unordered_map<std::string, SessionMapping> sessions_by_id_;
			std::unordered_map<std::string, std::string> client_to_session_;  // client_id -> session_id
			std::unordered_map<std::string, std::unordered_set<std::string>> server_sessions_;  // server_id -> session_ids
            
			mutable std::mutex mutex_;
            
			// Callbacks
			SessionCreatedCallback session_created_callback_;
			SessionExpiredCallback session_expired_callback_;
            
			// Internal helpers
			auto generate_session_id() const -> std::string;
			auto extract_client_identifier(const std::string& raw_identifier) const -> std::string;
			auto is_session_expired(const SessionMapping& session) const -> bool;
		};
        
		// Load balancer with sticky session support
		class StickyLoadBalancer
		{
		public:
			StickyLoadBalancer();
			~StickyLoadBalancer();
            
			// Enable sticky sessions
			auto enable_sticky_sessions(const StickySessionConfig& config) -> void;
			auto disable_sticky_sessions() -> void;
			auto is_sticky_enabled() const -> bool { return sticky_enabled_; }
            
			// Select server with sticky session support
			auto select_server(const std::string& client_identifier,
							   const std::vector<std::string>& available_servers,
							   const std::unordered_map<std::string, float>& server_loads = {}) 
				-> std::optional<std::string>;
            
			// Server management
			auto mark_server_down(const std::string& server_id) -> void;
			auto mark_server_up(const std::string& server_id) -> void;
            
			// Get sticky session manager
			auto get_session_manager() -> StickySessionManager& { return session_manager_; }
            
		private:
			StickySessionManager session_manager_;
			bool sticky_enabled_{false};
            
			std::unordered_set<std::string> down_servers_;
			mutable std::mutex servers_mutex_;
            
			// Fallback server selection (when sticky server is down)
			auto select_fallback_server(const std::vector<std::string>& available_servers,
										const std::unordered_map<std::string, float>& server_loads) 
				-> std::optional<std::string>;
		};
        
	}
}
