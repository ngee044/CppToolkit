#include "SessionSecurityManager.h"

#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <Generator.h>

using namespace Utilities;

namespace GameNetwork::Security
{
	auto SessionSecurityManager::initialize(const SessionSecurityPolicy& policy) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (is_initialized_)
		{
			return {false, "SecurityManager already initialized"};
		}
        
		try
		{
			policy_ = policy;
			active_sessions_.clear();
			sessions_by_account_.clear();
			token_to_session_.clear();
            
			is_initialized_ = true;
            
			Logger::handle().write(LogTypes::Information, "SessionSecurityManager initialized successfully");
			return {true, std::nullopt};
		}
		catch (const std::exception& e)
		{
			return {false, std::string("Failed to initialize: ") + e.what()};
		}
	}

	auto SessionSecurityManager::shutdown() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		active_sessions_.clear();
		sessions_by_account_.clear();
		token_to_session_.clear();
		is_initialized_ = false;
        
		Logger::handle().write(LogTypes::Information, "SessionSecurityManager shutdown complete");
	}

	auto SessionSecurityManager::check_concurrent_sessions(const std::string& account_id) -> std::vector<std::string>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return {};
		}
        
		cleanup_expired_sessions();
        
		auto it = sessions_by_account_.find(account_id);
		if (it != sessions_by_account_.end())
		{
			return it->second;
		}
        
		return {};
	}

	auto SessionSecurityManager::register_active_session(const std::string& account_id, const std::string& session_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return {false, "SecurityManager not initialized"};
		}
        
		try
		{
			// Add to sessions_by_account
			sessions_by_account_[account_id].push_back(session_id);
            
			// Create session info (will be populated when token is generated)
			SessionInfo info;
			info.session_id = session_id;
			info.account_id = account_id;
			info.created_time = std::chrono::steady_clock::now();
			info.last_activity = info.created_time;
            
			active_sessions_[session_id] = std::move(info);
            
			return {true, std::nullopt};
		}
		catch (const std::exception& e)
		{
			return {false, std::string("Failed to register session: ") + e.what()};
		}
	}

	auto SessionSecurityManager::unregister_session(const std::string& session_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto session_it = active_sessions_.find(session_id);
		if (session_it != active_sessions_.end())
		{
			const auto& account_id = session_it->second.account_id;
			const auto& token = session_it->second.current_token;
            
			// Remove from token mapping
			if (!token.empty())
			{
				token_to_session_.erase(token);
			}
            
			// Remove from account sessions
			auto account_it = sessions_by_account_.find(account_id);
			if (account_it != sessions_by_account_.end())
			{
				auto& session_list = account_it->second;
				session_list.erase(
					std::remove(session_list.begin(), session_list.end(), session_id),
					session_list.end()
				);
                
				if (session_list.empty())
				{
					sessions_by_account_.erase(account_it);
				}
			}
            
			// Remove session info
			active_sessions_.erase(session_it);
		}
	}

	auto SessionSecurityManager::generate_session_token(const std::string& session_id, const std::string& client_ip, const std::string& device_id) -> std::tuple<std::string, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return {"", "SecurityManager not initialized"};
		}
        
		auto it = active_sessions_.find(session_id);
		if (it == active_sessions_.end())
		{
			return {"", "Session not found"};
		}
        
		try
		{
			std::string token = generate_token();
            
			// Update session info
			it->second.client_ip = client_ip;
			it->second.device_id = device_id;
			it->second.current_token = token;
			it->second.last_activity = std::chrono::steady_clock::now();
            
			// Map token to session
			token_to_session_[token] = session_id;
            
			return {token, std::nullopt};
		}
		catch (const std::exception& e)
		{
			return {"", std::string("Failed to generate token: ") + e.what()};
		}
	}

	auto SessionSecurityManager::validate_session_token(const std::string& token, const std::string& session_id, const std::string& client_ip, const std::string& device_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return {false, "SecurityManager not initialized"};
		}
        
		auto token_it = token_to_session_.find(token);
		if (token_it == token_to_session_.end() || token_it->second != session_id)
		{
			return {false, "Invalid token"};
		}
        
		auto session_it = active_sessions_.find(session_id);
		if (session_it == active_sessions_.end())
		{
			return {false, "Session not found"};
		}
        
		auto& session_info = session_it->second;
        
		// Check if session is locked
		if (session_info.is_locked)
		{
			return {false, "Session is locked: " + session_info.lock_reason};
		}
        
		// Validate IP if enabled
		if (policy_.enable_ip_validation && session_info.client_ip != client_ip)
		{
			return {false, "IP address mismatch"};
		}
        
		// Validate device if enabled
		if (policy_.enable_device_validation && session_info.device_id != device_id)
		{
			return {false, "Device ID mismatch"};
		}
        
		// Check token validity
		if (!is_token_valid(token, session_info))
		{
			return {false, "Token expired"};
		}
        
		// Update last activity
		session_info.last_activity = std::chrono::steady_clock::now();
        
		return {true, std::nullopt};
	}

	auto SessionSecurityManager::refresh_session_token(const std::string& old_token) -> std::tuple<std::string, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return {"", "SecurityManager not initialized"};
		}
        
		auto token_it = token_to_session_.find(old_token);
		if (token_it == token_to_session_.end())
		{
			return {"", "Invalid token"};
		}
        
		auto session_it = active_sessions_.find(token_it->second);
		if (session_it == active_sessions_.end())
		{
			return {"", "Session not found"};
		}
        
		try
		{
			std::string new_token = generate_token();
            
			// Update session info
			session_it->second.current_token = new_token;
			session_it->second.last_activity = std::chrono::steady_clock::now();
            
			// Update token mapping
			token_to_session_.erase(old_token);
			token_to_session_[new_token] = session_it->first;
            
			return {new_token, std::nullopt};
		}
		catch (const std::exception& e)
		{
			return {"", fmt::format("Failed to refresh token: {}", e.what())};
		}
	}

	auto SessionSecurityManager::is_session_locked(const std::string& session_id) -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = active_sessions_.find(session_id);
		return it != active_sessions_.end() && it->second.is_locked;
	}

	auto SessionSecurityManager::check_session_timeout(const std::string& session_id) -> std::tuple<bool, uint32_t>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = active_sessions_.find(session_id);
		if (it == active_sessions_.end())
		{
			return {true, 0};
		}
        
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_activity);
        
		if (elapsed.count() >= policy_.session_timeout_minutes)
		{
			return {true, 0};
		}
        
		uint32_t remaining = policy_.session_timeout_minutes - static_cast<uint32_t>(elapsed.count());
		return {false, remaining};
	}

	auto SessionSecurityManager::lock_session(const std::string& session_id, const std::string& reason) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = active_sessions_.find(session_id);
		if (it != active_sessions_.end())
		{
			it->second.is_locked = true;
			it->second.lock_reason = reason;

			Logger::handle().write(LogTypes::Error, fmt::format("Session locked: {} - {}", session_id, reason));
		}
	}

	auto SessionSecurityManager::unlock_session(const std::string& session_id) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		auto it = active_sessions_.find(session_id);
		if (it != active_sessions_.end())
		{
			it->second.is_locked = false;
			it->second.lock_reason.clear();
			it->second.failed_attempts = 0;

			Logger::handle().write(LogTypes::Information, fmt::format("Session unlocked: {}", session_id));
		}
	}

	auto SessionSecurityManager::get_policy() const -> const SessionSecurityPolicy&
	{
		return policy_;
	}

	auto SessionSecurityManager::update_policy(const SessionSecurityPolicy& policy) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		policy_ = policy;

		Logger::handle().write(LogTypes::Information, fmt::format("Security policy updated"));
	}

	auto SessionSecurityManager::generate_token() -> std::string
	{
		// Generate a unique token using the Generator utility
		return Generator::guid();
	}

	auto SessionSecurityManager::is_token_valid(const std::string& token, const SessionInfo& session) -> bool
	{
		// Check if token matches
		if (session.current_token != token)
		{
			return false;
		}
        
		// Check token age
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - session.last_activity);
        
		return elapsed.count() < policy_.token_refresh_interval_minutes;
	}

	auto SessionSecurityManager::cleanup_expired_sessions() -> void
	{
		auto now = std::chrono::steady_clock::now();
        
		std::vector<std::string> expired_sessions;
        
		for (const auto& [session_id, session_info] : active_sessions_)
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - session_info.last_activity);
			if (elapsed.count() >= policy_.session_timeout_minutes)
			{
				expired_sessions.push_back(session_id);
			}
		}
        
		for (const auto& session_id : expired_sessions)
		{
			unregister_session(session_id);
			Logger::handle().write(LogTypes::Information, fmt::format("Cleaned up expired session: {}", session_id));
		}
	}

	auto SessionSecurityManager::cleanup_expired_tokens() -> uint32_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return 0;
		}
        
		auto now = std::chrono::steady_clock::now();
		uint32_t cleaned_count = 0;
        
		std::vector<std::string> expired_tokens;
        
		for (const auto& [token, session_id] : token_to_session_)
		{
			auto session_it = active_sessions_.find(session_id);
			if (session_it != active_sessions_.end())
			{
				auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
					now - session_it->second.last_activity);
                
				if (elapsed.count() >= policy_.token_refresh_interval_minutes)
				{
					expired_tokens.push_back(token);
					cleaned_count++;
				}
			}
			else
			{
				// Session doesn't exist, remove token
				expired_tokens.push_back(token);
				cleaned_count++;
			}
		}
        
		for (const auto& token : expired_tokens)
		{
			token_to_session_.erase(token);
		}
        
		return cleaned_count;
	}

	auto SessionSecurityManager::cleanup_inactive_sessions() -> uint32_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return 0;
		}
        
		auto now = std::chrono::steady_clock::now();
		uint32_t cleaned_count = 0;
        
		std::vector<std::string> inactive_sessions;
        
		for (const auto& [session_id, session_info] : active_sessions_)
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
				now - session_info.last_activity);
            
			if (elapsed.count() >= policy_.session_timeout_minutes)
			{
				inactive_sessions.push_back(session_id);
				cleaned_count++;
			}
		}
        
		for (const auto& session_id : inactive_sessions)
		{
			unregister_session(session_id);
		}
        
		return cleaned_count;
	}

	auto SessionSecurityManager::get_idle_sessions(uint32_t timeout_minutes) -> std::vector<std::string>
	{
		std::lock_guard<std::mutex> lock(mutex_);
        
		if (!is_initialized_)
		{
			return {};
		}
        
		auto now = std::chrono::steady_clock::now();
		std::vector<std::string> idle_sessions;
        
		for (const auto& [session_id, session_info] : active_sessions_)
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
				now - session_info.last_activity);
            
			if (elapsed.count() >= timeout_minutes)
			{
				idle_sessions.push_back(session_id);
			}
		}
        
		return idle_sessions;
	}
}
