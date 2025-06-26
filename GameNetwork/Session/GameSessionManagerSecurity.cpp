#include "GameSessionManagerSecurity.h"
#include "GameSessionManager.h"
#include "../Security/SessionSecurityManager.h"
#include "../../Utilities/Logger.h"
#include "../../Utilities/Generator.h"

using namespace Utilities;

namespace GameNetwork
{
    auto GameSessionManager::initialize_with_security(std::shared_ptr<Thread::ThreadPool> thread_pool,
                                                     const Security::SessionSecurityPolicy& security_policy) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // First perform basic initialization
        auto [init_success, init_error] = initialize(thread_pool);
        if (!init_success)
        {
            return {false, init_error};
        }
        
        try
        {
            // Initialize security manager
            security_manager_ = std::make_unique<Security::SessionSecurityManager>();
            auto [sec_success, sec_error] = security_manager_->initialize(security_policy);
            if (!sec_success)
            {
                shutdown();            return {false, "Failed to initialize security: " + sec_error.value_or("Unknown error")};
        }
        
        Logger::handle().write(LogTypes::Information, "GameSessionManager initialized with security features");
        return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            shutdown();
            return {false, std::string("Failed to initialize security: ") + e.what()};
        }
    }

    auto GameSessionManager::on_network_connected_secure(std::shared_ptr<Network::NetworkSession> network_session, 
                                                        const std::string& account_id,
                                                        const std::string& client_ip,
                                                        const std::string& device_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            // Fallback to basic connection if security not initialized
            return on_network_connected(network_session, account_id);
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        try
        {
            // Check concurrent sessions
            auto existing_sessions = security_manager_->check_concurrent_sessions(account_id);
            if (!existing_sessions.empty())
            {
                auto policy = security_manager_->get_policy();
                if (existing_sessions.size() >= policy.max_concurrent_sessions_per_account)
                {
                    if (!policy.allow_kick_previous_session)
                    {
                        return {false, "Maximum concurrent sessions reached"};
                    }
                    
                    // Kick previous sessions
                    for (const auto& session_id : existing_sessions)
                    {
                        if (auto session = get_session_by_id(session_id))
                        {
                            session->set_kicked_by_duplicate_login(true);
                            terminate_session(session_id);
                            Logger::handle().write(LogTypes::Information, 
                                "Kicked previous session for duplicate login: " + session_id);
                        }
                    }
                }
            }
            
            // Create or restore session
            auto [session, error] = create_or_restore_session(account_id);
            if (!session)
            {
                return {false, error};
            }
            
            // Generate security token
            auto [token, token_error] = security_manager_->generate_session_token(
                session->session_id(), client_ip, device_id);
            if (token.empty())
            {
                terminate_session(session->session_id());
                return {false, "Failed to generate security token: " + token_error.value_or("Unknown error")};
            }
            
            // Register active session
            auto [reg_success, reg_error] = security_manager_->register_active_session(
                account_id, session->session_id());
            if (!reg_success)
            {
                terminate_session(session->session_id());
                return {false, reg_error};
            }
            
            // Create connection
            auto connection = std::make_shared<GameConnection>(network_session);
            
            // Bind connection to session
            session->bind_connection(connection);
            session->set_session_token(token);
            
            // Store connection
            connections_by_id_[connection->id()] = connection;
            connection_to_session_[connection->id()] = session->session_id();
            
            // Update statistics
            update_peak_sessions();
            
            // Notify callbacks
            notify_session_connected(session);
            
            Logger::handle().write(LogTypes::Information, 
                "Secure connection established for account: " + account_id);
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {false, std::string("Connection failed: ") + e.what()};
        }
    }

    auto GameSessionManager::validate_session_token(const std::string& session_id,
                                                   const std::string& token,
                                                   const std::string& client_ip,
                                                   const std::string& device_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            return {true, std::nullopt};  // No security check if not initialized
        }
        
        return security_manager_->validate_session_token(token, session_id, client_ip, device_id);
    }

    auto GameSessionManager::refresh_session_token(const std::string& session_id,
                                                  const std::string& old_token) 
        -> std::tuple<std::string, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            return {old_token, std::nullopt};  // Return same token if security not initialized
        }
        
        auto [new_token, error] = security_manager_->refresh_session_token(old_token);
        if (!new_token.empty())
        {
            if (auto session = get_session_by_id(session_id))
            {
                session->set_session_token(new_token);
            }
        }
        
        return {new_token, error};
    }

    auto GameSessionManager::check_session_security(const std::string& session_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!security_manager_)
        {
            return {true, std::nullopt};
        }
        
        // Check if session is locked
        if (security_manager_->is_session_locked(session_id))
        {
            return {false, "Session is locked due to security violations"};
        }
        
        // Check timeout
        auto [is_timeout, remaining_time] = security_manager_->check_session_timeout(session_id);
        if (is_timeout)
        {
            return {false, "Session has timed out"};
        }
        
        return {true, std::nullopt};
    }

    auto GameSessionManager::get_active_sessions_for_account(const std::string& account_id) 
        -> std::vector<std::string>
    {
        if (!security_manager_)
        {
            // Fallback to basic session lookup
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = sessions_by_account_.find(account_id);
            if (it != sessions_by_account_.end())
            {
                return {it->second->session_id()};
            }
            return {};
        }
        
        return security_manager_->check_concurrent_sessions(account_id);
    }
}