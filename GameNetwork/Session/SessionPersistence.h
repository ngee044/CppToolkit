#pragma once

#include <GameSession.h>
#include "../Core/Location.h"
#include <RedisClient.h>
#include <memory>
#include <string>
#include <optional>
#include <tuple>

namespace GameNetwork
{
    struct SessionData
    {
        std::string session_id;
        std::string account_id;
        uint64_t character_id;
        GameNetwork::Location location;
        uint32_t channel_id;
        std::chrono::steady_clock::time_point last_activity;
        std::unordered_map<std::string, std::any> custom_data;
    };
    
    class SessionPersistence
    {
    public:
        SessionPersistence();
        ~SessionPersistence();
        
        auto initialize() -> std::tuple<bool, std::optional<std::string>>;
        auto save_session(const std::string& session_id, const SessionData& data) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto load_session(const std::string& session_id) 
            -> std::tuple<SessionData, std::optional<std::string>>;
        
    private:
        std::shared_ptr<Redis::RedisClient> redis_client_;
    };
}
