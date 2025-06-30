#include "StickySession.h"

#include <Logger.h>
#include <Generator.h>

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <boost/json.hpp>

#include <algorithm>
#include <random>
#include <chrono>

using namespace Utilities;

namespace GameNetwork
{
    namespace LoadBalancing
    {
        StickySessionManager::StickySessionManager(const StickySessionConfig& config)
            : config_(config)
        {
        }
        
        StickySessionManager::~StickySessionManager() = default;
        
        auto StickySessionManager::set_config(const StickySessionConfig& config) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            config_ = config;
        }
        
        auto StickySessionManager::get_server_for_session(const std::string& client_identifier, const std::vector<std::string>& available_servers) 
            -> std::optional<std::string>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto identifier = extract_client_identifier(client_identifier);
            
            auto client_it = client_to_session_.find(identifier);
            if (client_it != client_to_session_.end())
            {
                auto session_it = sessions_by_id_.find(client_it->second);
                if (session_it != sessions_by_id_.end())
                {
                    if (!is_session_expired(session_it->second))
                    {
                        auto& mapping = session_it->second;
                        if (std::find(available_servers.begin(), available_servers.end(), mapping.server_id) != available_servers.end())
                        {
                            mapping.last_access_time = std::chrono::steady_clock::now();
                            mapping.request_count++;
                            
                            return mapping.server_id;
                        }
                        else if (!config_.enable_fallback)
                        {
                            return std::nullopt;
                        }
                    }
                    else
                    {
                        // Session expired, remove it
                        auto session_id = session_it->first;
                        remove_session(session_id);
                        
                        if (session_expired_callback_)
                        {
                            session_expired_callback_(session_id);
                        }
                    }
                }
            }
            
            return std::nullopt;  // No existing session
        }

        auto StickySessionManager::create_session_mapping(const std::string& client_identifier, const std::string& server_id) 
            -> std::string
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto identifier = extract_client_identifier(client_identifier);
            auto session_id = generate_session_id();
            auto now = std::chrono::steady_clock::now();
            
            // Create session mapping
            SessionMapping mapping;
            mapping.session_id = session_id;
            mapping.server_id = server_id;
            mapping.client_identifier = identifier;
            mapping.created_time = now;
            mapping.last_access_time = now;
            mapping.request_count = 1;
            
            // Store mapping
            sessions_by_id_[session_id] = mapping;
            client_to_session_[identifier] = session_id;
            // server_sessions_[server_id].insert(session_id);
            
            // Notify callback
            if (session_created_callback_)
            {
                session_created_callback_(session_id, server_id);
            }

            Logger::handle().write(LogTypes::Information, fmt::format("Created sticky session {} for client {} on server {}", session_id, identifier, server_id));

            return session_id;
        }
        
        auto StickySessionManager::touch_session(const std::string& session_id) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = sessions_by_id_.find(session_id);
            if (it != sessions_by_id_.end())
            {
                it->second.last_access_time = std::chrono::steady_clock::now();
                it->second.request_count++;
            }
        }
        
        auto StickySessionManager::remove_session(const std::string& session_id) -> void
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = sessions_by_id_.find(session_id);
            if (it != sessions_by_id_.end())
            {
                auto& mapping = it->second;
                
                client_to_session_.erase(mapping.client_identifier);
                
                auto server_it = server_sessions_.find(mapping.server_id);
                if (server_it != server_sessions_.end())
                {
                    server_it->second.erase(session_id);
                    if (server_it->second.empty())
                    {
                        server_sessions_.erase(server_it);
                    }
                }
                
                sessions_by_id_.erase(it);
            }
        }
        
        auto StickySessionManager::remove_sessions_for_server(const std::string& server_id) -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            uint32_t removed_count = 0;
            
            auto server_it = server_sessions_.find(server_id);
            if (server_it != server_sessions_.end())
            {
                auto sessions_to_remove = server_it->second;
                
                for (const auto& session_id : sessions_to_remove)
                {
                    remove_session(session_id);
                    removed_count++;
                }
                
                server_sessions_.erase(server_it);
            }
            
            return removed_count;
        }
        
        auto StickySessionManager::cleanup_expired_sessions() -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            uint32_t removed_count = 0;
            std::vector<std::string> expired_sessions;
            
            for (const auto& [session_id, mapping] : sessions_by_id_)
            {
                if (is_session_expired(mapping))
                {
                    expired_sessions.push_back(session_id);
                }
            }
            
            for (const auto& session_id : expired_sessions)
            {
                remove_session(session_id);
                removed_count++;
                
                if (session_expired_callback_)
                {
                    session_expired_callback_(session_id);
                }
            }
            
            if (removed_count > 0)
            {
                Logger::handle().write(LogTypes::Information,
                    fmt::format("Cleaned up {} expired sessions", removed_count));
            }
            
            return removed_count;
        }
        
        auto StickySessionManager::generate_session_id() const -> std::string
        {
            return "[Session]" + Generator::guid();
        }
        
        auto StickySessionManager::extract_client_identifier(const std::string& raw_identifier) const 
            -> std::string
        {
            return raw_identifier;
        }
        
        auto StickySessionManager::is_session_expired(const SessionMapping& session) const -> bool
        {
            auto now = std::chrono::steady_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - session.last_access_time);
            return age > config_.session_timeout;
        }
        
        StickyLoadBalancer::StickyLoadBalancer() = default;
        StickyLoadBalancer::~StickyLoadBalancer() = default;
        
        auto StickyLoadBalancer::enable_sticky_sessions(const StickySessionConfig& config) -> void
        {
            session_manager_.set_config(config);
            sticky_enabled_ = true;
        }
        
        auto StickyLoadBalancer::disable_sticky_sessions() -> void
        {
            sticky_enabled_ = false;
        }
        
        auto StickyLoadBalancer::select_server(const std::string& client_identifier,
                                               const std::vector<std::string>& available_servers,
                                               const std::unordered_map<std::string, float>& server_loads) 
            -> std::optional<std::string>
        {
            if (!sticky_enabled_)
            {
                return select_fallback_server(available_servers, server_loads);
            }
            
            // Check for existing sticky session
            auto sticky_server = session_manager_.get_server_for_session(client_identifier, available_servers);
            if (sticky_server.has_value())
            {
                return sticky_server;
            }
            
            // No existing session, select new server
            auto selected_server = select_fallback_server(available_servers, server_loads);
            if (selected_server.has_value())
            {
                // Create sticky session
                session_manager_.create_session_mapping(client_identifier, *selected_server);
            }
            
            return selected_server;
        }

        auto StickyLoadBalancer::select_fallback_server(const std::vector<std::string>& available_servers, const std::unordered_map<std::string, float>& server_loads)
            -> std::optional<std::string>
        {
            if (available_servers.empty())
            {
                return std::nullopt;
            }
            
            // Filter out down servers
            std::vector<std::string> up_servers;
            {
                std::lock_guard<std::mutex> lock(servers_mutex_);
                for (const auto& server : available_servers)
                {
                    if (down_servers_.find(server) == down_servers_.end())
                    {
                        up_servers.push_back(server);
                    }
                }
            }
            
            if (up_servers.empty())
            {
                return std::nullopt;
            }
            
            // If no load information, random selection
            if (server_loads.empty())
            {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, up_servers.size() - 1);
                return up_servers[dis(gen)];
            }
            
            // Select server with lowest load
            std::string best_server = up_servers[0];
            float min_load = 1.0f;
            
            for (const auto& server : up_servers)
            {
                auto load_it = server_loads.find(server);
                float load = (load_it != server_loads.end()) ? load_it->second : 0.5f;
                
                if (load < min_load)
                {
                    min_load = load;
                    best_server = server;
                }
            }
            
            return best_server;
        }
        
    }
}
