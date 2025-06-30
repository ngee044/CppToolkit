#include "SessionPersistence.h"
#include <Logger.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <boost/json.hpp>
#include <boost/system/error_code.hpp>

#include <fstream>
#include <chrono>

using namespace Utilities;

namespace GameNetwork
{
    SessionPersistence::SessionPersistence()
    {
        redis_client_ = std::make_shared<Redis::RedisClient>("127.0.0.1", 6379);
    }

    SessionPersistence::~SessionPersistence() = default;

    auto SessionPersistence::initialize() -> std::tuple<bool, std::optional<std::string>>
    {
        // Connect to Redis (no parameters)
        auto result = redis_client_->connect();
        if (!std::get<0>(result))
        {
            auto error_msg = fmt::format("Failed to connect to Redis: {}", std::get<1>(result).value_or("Unknown error"));
            Logger::handle().write(LogTypes::Error, error_msg);
            return {false, error_msg};
        }
        
        Logger::handle().write(LogTypes::Information, fmt::format("SessionPersistence initialized successfully"));
        return { true, std::nullopt };
    }

    auto SessionPersistence::save_session(const std::string& session_id, const SessionData& data) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            // Serialize session data to JSON
            boost::json::object json_data;
            json_data["session_id"] = data.session_id;
            json_data["account_id"] = data.account_id;
            json_data["character_id"] = data.character_id;
            
            // Serialize location
            boost::json::object location_data;
            location_data["x"] = data.location.position.x;
            location_data["y"] = data.location.position.y;
            location_data["z"] = data.location.position.z;
            location_data["pitch"] = data.location.pitch;
            location_data["yaw"] = data.location.yaw;
            location_data["roll"] = data.location.roll;
            location_data["map_id"] = data.location.map_id;
            json_data["location"] = location_data;
            
            json_data["channel_id"] = data.channel_id;
            
            // Convert time_point to timestamp
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                data.last_activity.time_since_epoch()).count();
            json_data["last_activity"] = timestamp;
            
            // Convert to string
            std::string serialized = boost::json::serialize(json_data);
            
            // Save to Redis with TTL of 24 hours
            auto save_result = redis_client_->set("session:" + session_id, serialized, 86400);
            if (!std::get<0>(save_result))
            {
                return {false, "Failed to save session to Redis: " + std::get<1>(save_result).value_or("Unknown error")};
            }
            
            Logger::handle().write(LogTypes::Debug, fmt::format("Session saved: {}", session_id));
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return {false, std::string("Exception saving session: ") + e.what()};
        }
    }

    auto SessionPersistence::load_session(const std::string& session_id) 
        -> std::tuple<SessionData, std::optional<std::string>>
    {
        try
        {
            // Load from Redis
            auto load_result = redis_client_->get("session:" + session_id);
            if (std::get<1>(load_result).has_value())
            {
                return { SessionData{}, fmt::format("Session not found: {}", std::get<1>(load_result).value()) };
            }
            
            std::string json_string = std::get<0>(load_result);
            if (json_string.empty())
            {
                return { SessionData{}, "Session not found"};
            }
            
            // Parse JSON
            boost::system::error_code ec;
            auto json_value = boost::json::parse(json_string, ec);
            if (ec)
            {
                return { SessionData{}, fmt::format("Failed to parse session data: {}", ec.message()) };
            }
            
            auto json_data = json_value.as_object();
            
            // Deserialize session data
            SessionData data;
            data.session_id = boost::json::value_to<std::string>(json_data.at("session_id"));
            data.account_id = boost::json::value_to<std::string>(json_data.at("account_id"));
            data.character_id = boost::json::value_to<uint64_t>(json_data.at("character_id"));
            
            // Deserialize location
            if (json_data.contains("location") && json_data.at("location").is_object())
            {
                auto location_obj = json_data.at("location").as_object();
                
                if (location_obj.contains("x"))
                {
                    data.location.position.x = static_cast<float>(location_obj.at("x").as_double());
                }
                if (location_obj.contains("y"))
                {
                    data.location.position.y = static_cast<float>(location_obj.at("y").as_double());
                }
                if (location_obj.contains("z"))
                {
                    data.location.position.z = static_cast<float>(location_obj.at("z").as_double());
                }
                data.location.x = data.location.position.x;
                data.location.y = data.location.position.y;
                data.location.z = data.location.position.z;
                
                if (location_obj.contains("pitch"))
                {
                    data.location.pitch = static_cast<float>(location_obj.at("pitch").as_double());
                }
                if (location_obj.contains("yaw"))
                {
                    data.location.yaw = static_cast<float>(location_obj.at("yaw").as_double());
                }
                if (location_obj.contains("roll"))
                {
                    data.location.roll = static_cast<float>(location_obj.at("roll").as_double());
                }
                if (location_obj.contains("map_id"))
                {
                    data.location.map_id = static_cast<uint32_t>(location_obj.at("map_id").as_int64());
                }
            }
            
            data.channel_id = boost::json::value_to<uint32_t>(json_data.at("channel_id"));
            
            // Convert timestamp to time_point
            auto timestamp = boost::json::value_to<int64_t>(json_data.at("last_activity"));
            data.last_activity = std::chrono::steady_clock::time_point(
                std::chrono::seconds(timestamp));

            Logger::handle().write(LogTypes::Debug, fmt::format("Session loaded: {}", session_id));

            return { data, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { SessionData{}, fmt::format("Exception loading session: {}", e.what()) };
        }
    }
}
