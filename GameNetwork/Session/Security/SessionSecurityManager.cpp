#include "SessionSecurityManager.h"
#include "../../../Utilities/Logger.h"
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <algorithm>

namespace GameNetwork
{
    namespace Security
    {
        SessionSecurityManager::SessionSecurityManager()
            : rng_(std::chrono::steady_clock::now().time_since_epoch().count())
            , dist_(0, std::numeric_limits<uint64_t>::max())
        {
        }

        SessionSecurityManager::~SessionSecurityManager() = default;

        auto SessionSecurityManager::initialize(const SessionSecurityPolicy& policy)
            -> std::tuple<bool, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            try
            {
                policy_ = policy;
                
                // Clear any existing data
                tokens_.clear();
                session_tokens_.clear();
                active_sessions_.clear();
                session_activity_.clear();
                session_start_time_.clear();
                failed_validations_.clear();
                lockout_times_.clear();
                
                Utilities::Logger::info("SessionSecurityManager initialized with policy settings");
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                return {false, std::string("Failed to initialize SessionSecurityManager: ") + e.what()};
            }
        }

        auto SessionSecurityManager::generate_session_token(const std::string& session_id,
                                                           const std::string& client_ip,
                                                           const std::string& device_id)
            -> std::tuple<std::string, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            try
            {
                // Check if session is locked
                if (is_session_locked(session_id))
                {
                    return {"", "Session is locked due to security violations"};
                }
                
                // Generate secure token
                auto token = generate_secure_token();
                auto hashed_token = hash_token(token);
                
                // Create token data
                SessionToken token_data;
                token_data.token = token;
                token_data.session_id = session_id;
                token_data.client_ip = client_ip;
                token_data.device_id = device_id;
                token_data.created_time = std::chrono::steady_clock::now();
                token_data.last_validated = token_data.created_time;
                token_data.validation_count = 0;
                
                // Store token
                tokens_[hashed_token] = token_data;
                session_tokens_[session_id] = hashed_token;
                
                // Update session activity
                update_session_activity(session_id);
                
                Utilities::Logger::debug("Generated session token for session: " + session_id);
                return {token, std::nullopt};
            }
            catch (const std::exception& e)
            {
                return {"", std::string("Failed to generate session token: ") + e.what()};
            }
        }

        auto SessionSecurityManager::validate_session_token(const std::string& token,
                                                           const std::string& session_id,
                                                           const std::string& client_ip,
                                                           const std::string& device_id)
            -> std::tuple<bool, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            try
            {
                // Check if session is locked
                if (is_session_locked(session_id))
                {
                    return {false, "Session is locked due to security violations"};
                }
                
                auto hashed_token = hash_token(token);
                auto it = tokens_.find(hashed_token);
                
                if (it == tokens_.end())
                {
                    record_validation_failure(session_id);
                    return {false, "Invalid token"};
                }
                
                auto& token_data = it->second;
                
                // Check token expiration
                auto now = std::chrono::steady_clock::now();
                if (now - token_data.created_time > policy_.token_lifetime)
                {
                    tokens_.erase(it);
                    session_tokens_.erase(session_id);
                    return {false, "Token expired"};
                }
                
                // Verify session ID
                if (token_data.session_id != session_id)
                {
                    record_validation_failure(session_id);
                    return {false, "Token session mismatch"};
                }
                
                // Verify constraints if enabled
                if (!verify_token_constraints(token_data, client_ip, device_id))
                {
                    record_validation_failure(session_id);
                    
                    if (policy_.enable_session_hijacking_protection)
                    {
                        Utilities::Logger::warning("Possible session hijacking attempt detected for session: " + session_id);
                    }
                    
                    return {false, "Token validation constraints failed"};
                }
                
                // Update validation data
                token_data.last_validated = now;
                token_data.validation_count++;
                
                // Update session activity
                update_session_activity(session_id);
                
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                return {false, std::string("Token validation error: ") + e.what()};
            }
        }

        auto SessionSecurityManager::refresh_session_token(const std::string& old_token)
            -> std::tuple<std::string, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            try
            {
                auto hashed_token = hash_token(old_token);
                auto it = tokens_.find(hashed_token);
                
                if (it == tokens_.end())
                {
                    return {"", "Invalid token"};
                }
                
                auto& token_data = it->second;
                
                // Check if refresh is needed
                auto now = std::chrono::steady_clock::now();
                if (now - token_data.last_validated < policy_.token_refresh_interval)
                {
                    return {old_token, std::nullopt};  // No refresh needed yet
                }
                
                // Generate new token
                auto new_token = generate_secure_token();
                auto new_hashed_token = hash_token(new_token);
                
                // Copy token data with new token
                SessionToken new_token_data = token_data;
                new_token_data.token = new_token;
                new_token_data.created_time = now;
                new_token_data.last_validated = now;
                
                // Update storage
                tokens_.erase(it);
                tokens_[new_hashed_token] = new_token_data;
                session_tokens_[token_data.session_id] = new_hashed_token;
                
                return {new_token, std::nullopt};
            }
            catch (const std::exception& e)
            {
                return {"", std::string("Failed to refresh token: ") + e.what()};
            }
        }

        auto SessionSecurityManager::revoke_session_token(const std::string& token) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto hashed_token = hash_token(token);
            auto it = tokens_.find(hashed_token);
            
            if (it != tokens_.end())
            {
                session_tokens_.erase(it->second.session_id);
                tokens_.erase(it);
            }
        }

        auto SessionSecurityManager::detect_hijacking_attempt(const std::string& session_id,
                                                             const std::string& client_ip,
                                                             const std::string& device_id)
            -> bool
        {
            if (!policy_.enable_session_hijacking_protection)
            {
                return false;
            }
            
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = session_tokens_.find(session_id);
            if (it == session_tokens_.end())
            {
                return false;
            }
            
            auto token_it = tokens_.find(it->second);
            if (token_it == tokens_.end())
            {
                return false;
            }
            
            const auto& token_data = token_it->second;
            
            // Check IP change
            if (policy_.require_ip_match && token_data.client_ip != client_ip)
            {
                return true;
            }
            
            // Check device change
            if (policy_.require_device_match && token_data.device_id != device_id)
            {
                return true;
            }
            
            return false;
        }

        auto SessionSecurityManager::record_validation_failure(const std::string& session_id) -> void
        {
            failed_validations_[session_id]++;
            
            if (failed_validations_[session_id] >= policy_.max_failed_validations)
            {
                lockout_times_[session_id] = std::chrono::steady_clock::now();
                Utilities::Logger::warning("Session locked due to excessive validation failures: " + session_id);
            }
        }

        auto SessionSecurityManager::is_session_locked(const std::string& session_id) const -> bool
        {
            auto it = lockout_times_.find(session_id);
            if (it == lockout_times_.end())
            {
                return false;
            }
            
            auto elapsed = std::chrono::steady_clock::now() - it->second;
            return elapsed < policy_.lockout_duration;
        }

        auto SessionSecurityManager::check_concurrent_sessions(const std::string& account_id)
            -> std::vector<std::string>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = active_sessions_.find(account_id);
            if (it == active_sessions_.end())
            {
                return {};
            }
            
            return std::vector<std::string>(it->second.begin(), it->second.end());
        }

        auto SessionSecurityManager::register_active_session(const std::string& account_id,
                                                            const std::string& session_id)
            -> std::tuple<bool, std::optional<std::string>>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            try
            {
                auto& sessions = active_sessions_[account_id];
                
                // Check concurrent session limit
                if (sessions.size() >= policy_.max_concurrent_sessions_per_account)
                {
                    if (!policy_.allow_kick_previous_session)
                    {
                        return {false, "Maximum concurrent sessions reached for account"};
                    }
                    
                    // Kick oldest session (first in set)
                    if (!sessions.empty())
                    {
                        auto oldest_session = *sessions.begin();
                        sessions.erase(sessions.begin());
                        
                        // Clean up related data
                        session_tokens_.erase(oldest_session);
                        session_activity_.erase(oldest_session);
                        session_start_time_.erase(oldest_session);
                        
                        Utilities::Logger::info("Kicked previous session due to concurrent limit: " + oldest_session);
                    }
                }
                
                sessions.insert(session_id);
                session_start_time_[session_id] = std::chrono::steady_clock::now();
                
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                return {false, std::string("Failed to register active session: ") + e.what()};
            }
        }

        auto SessionSecurityManager::unregister_active_session(const std::string& account_id,
                                                               const std::string& session_id) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = active_sessions_.find(account_id);
            if (it != active_sessions_.end())
            {
                it->second.erase(session_id);
                if (it->second.empty())
                {
                    active_sessions_.erase(it);
                }
            }
            
            // Clean up related data
            session_tokens_.erase(session_id);
            session_activity_.erase(session_id);
            session_start_time_.erase(session_id);
            failed_validations_.erase(session_id);
            lockout_times_.erase(session_id);
        }

        auto SessionSecurityManager::get_session_count(const std::string& account_id) const -> size_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = active_sessions_.find(account_id);
            return (it != active_sessions_.end()) ? it->second.size() : 0;
        }

        auto SessionSecurityManager::update_session_activity(const std::string& session_id) -> void
        {
            session_activity_[session_id] = std::chrono::steady_clock::now();
        }

        auto SessionSecurityManager::check_session_timeout(const std::string& session_id)
            -> std::tuple<bool, std::chrono::seconds>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto now = std::chrono::steady_clock::now();
            
            // Check idle timeout
            auto activity_it = session_activity_.find(session_id);
            if (activity_it != session_activity_.end())
            {
                auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - activity_it->second);
                if (idle_time >= policy_.idle_timeout)
                {
                    return {true, std::chrono::seconds(0)};
                }
                
                auto remaining_idle = policy_.idle_timeout - idle_time;
                
                // Check absolute timeout
                auto start_it = session_start_time_.find(session_id);
                if (start_it != session_start_time_.end())
                {
                    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(now - start_it->second);
                    if (total_time >= policy_.absolute_timeout)
                    {
                        return {true, std::chrono::seconds(0)};
                    }
                    
                    auto remaining_absolute = policy_.absolute_timeout - total_time;
                    return {false, std::min(remaining_idle, remaining_absolute)};
                }
                
                return {false, remaining_idle};
            }
            
            return {true, std::chrono::seconds(0)};
        }

        auto SessionSecurityManager::get_idle_sessions(std::chrono::seconds idle_threshold) const
            -> std::vector<std::string>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            std::vector<std::string> idle_sessions;
            auto now = std::chrono::steady_clock::now();
            
            for (const auto& [session_id, last_activity] : session_activity_)
            {
                auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity);
                if (idle_time >= idle_threshold)
                {
                    idle_sessions.push_back(session_id);
                }
            }
            
            return idle_sessions;
        }

        auto SessionSecurityManager::update_policy(const SessionSecurityPolicy& policy) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            policy_ = policy;
        }

        auto SessionSecurityManager::get_policy() const -> SessionSecurityPolicy
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return policy_;
        }

        auto SessionSecurityManager::cleanup_expired_tokens() -> size_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            size_t removed = 0;
            auto now = std::chrono::steady_clock::now();
            
            for (auto it = tokens_.begin(); it != tokens_.end();)
            {
                if (now - it->second.created_time > policy_.token_lifetime)
                {
                    session_tokens_.erase(it->second.session_id);
                    it = tokens_.erase(it);
                    removed++;
                }
                else
                {
                    ++it;
                }
            }
            
            return removed;
        }

        auto SessionSecurityManager::cleanup_inactive_sessions() -> size_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            size_t removed = 0;
            auto now = std::chrono::steady_clock::now();
            
            // Clean up based on idle timeout
            for (auto it = session_activity_.begin(); it != session_activity_.end();)
            {
                auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - it->second);
                if (idle_time >= policy_.idle_timeout)
                {
                    auto session_id = it->first;
                    
                    // Find and remove from active sessions
                    for (auto& [account_id, sessions] : active_sessions_)
                    {
                        sessions.erase(session_id);
                    }
                    
                    // Clean up all related data
                    session_tokens_.erase(session_id);
                    session_start_time_.erase(session_id);
                    failed_validations_.erase(session_id);
                    lockout_times_.erase(session_id);
                    
                    it = session_activity_.erase(it);
                    removed++;
                }
                else
                {
                    ++it;
                }
            }
            
            // Clean up empty account entries
            for (auto it = active_sessions_.begin(); it != active_sessions_.end();)
            {
                if (it->second.empty())
                {
                    it = active_sessions_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            
            return removed;
        }

        auto SessionSecurityManager::generate_secure_token() -> std::string
        {
            std::stringstream ss;
            
            // Generate 256-bit random value
            for (int i = 0; i < 4; ++i)
            {
                ss << std::hex << std::setw(16) << std::setfill('0') << dist_(rng_);
            }
            
            return ss.str();
        }

        auto SessionSecurityManager::hash_token(const std::string& token) const -> std::string
        {
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char*>(token.c_str()), token.length(), hash);
            
            std::stringstream ss;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }
            
            return ss.str();
        }

        auto SessionSecurityManager::verify_token_constraints(const SessionToken& token,
                                                             const std::string& client_ip,
                                                             const std::string& device_id) const -> bool
        {
            if (policy_.require_ip_match && token.client_ip != client_ip)
            {
                return false;
            }
            
            if (policy_.require_device_match && token.device_id != device_id)
            {
                return false;
            }
            
            return true;
        }
    }
}