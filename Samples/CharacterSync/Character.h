#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <boost/json.hpp>
#include <boost/system.hpp>
#include "../../Utilities/Logger.h"
#include "../../Redis/RedisClient.h"

namespace Samples
{
    // Example Location struct for demonstration
    struct Location
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        uint32_t map_id = 0;
    };

    // Example Character class demonstrating GameNetwork usage with Redis persistence
    class Character
    {
    public:
        Character(uint64_t character_id) : character_id_(character_id), id(character_id) {}
        
        Character(uint64_t character_id, const std::string& name) 
            : character_id_(character_id), id(character_id), name_(name) {}
        
        auto get_location() const -> Location 
        { 
            return location_; 
        }
        
        auto set_location(const Location& loc) -> void 
        { 
            location_ = loc; 
        }
        
        auto get_name() const -> std::string
        {
            return name_;
        }
        
        auto set_name(const std::string& name) -> void
        {
            name_ = name;
        }
        
        auto save() -> std::tuple<bool, std::optional<std::string>>
        {
            try
            {
                // Serialize character data to JSON
                boost::json::object char_data;
                char_data["character_id"] = character_id_;
                char_data["name"] = name_;
                char_data["location"] = boost::json::object{
                    {"x", location_.x},
                    {"y", location_.y},
                    {"z", location_.z},
                    {"map_id", location_.map_id}
                };
                
                // Save to Redis
                auto redis_client = std::make_shared<Redis::RedisClient>("127.0.0.1", 6379);
                auto [connect_success, connect_error] = redis_client->connect();
                if (!connect_success)
                {
                    return {false, "Failed to connect to Redis: " + connect_error.value_or("Unknown error")};
                }
                
                std::string key = "character:" + std::to_string(character_id_);
                std::string json_str = boost::json::serialize(char_data);
                
                auto [set_success, set_error] = redis_client->set(key, json_str);
                if (!set_success)
                {
                    return {false, "Failed to save character: " + set_error.value_or("Unknown error")};
                }
                
                Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                    "Character saved: " + std::to_string(character_id_));
                
                return {true, std::nullopt};
            }
            catch (const std::exception& e)
            {
                return {false, std::string("Exception during save: ") + e.what()};
            }
        }
        
        static auto load(uint64_t character_id, const std::string& name) -> std::tuple<std::shared_ptr<Character>, std::optional<std::string>>
        {
            try
            {
                // Try to load from Redis first
                auto redis_client = std::make_shared<Redis::RedisClient>("127.0.0.1", 6379);
                auto [connect_success, connect_error] = redis_client->connect();
                if (!connect_success)
                {
                    // If Redis is not available, create new character
                    return {std::make_shared<Character>(character_id, name), std::nullopt};
                }
                
                std::string key = "character:" + std::to_string(character_id);
                auto [get_result, get_error] = redis_client->get(key);
                
                if (!get_error.has_value() && !get_result.empty())
                {
                    // Parse JSON and load character
                    boost::system::error_code ec;
                    auto json_value = boost::json::parse(get_result, ec);
                    
                    if (ec)
                    {
                        return {std::make_shared<Character>(character_id, name), 
                               "Failed to parse character data: " + ec.message()};
                    }
                    
                    auto json_obj = json_value.as_object();
                    auto loaded_char = std::make_shared<Character>(character_id, name);
                    
                    // Load name if present
                    if (json_obj.contains("name"))
                    {
                        loaded_char->name_ = json_obj["name"].as_string().c_str();
                    }
                    
                    // Load location if present
                    if (json_obj.contains("location"))
                    {
                        auto loc_obj = json_obj["location"].as_object();
                        loaded_char->location_.x = static_cast<float>(loc_obj["x"].as_double());
                        loaded_char->location_.y = static_cast<float>(loc_obj["y"].as_double());
                        loaded_char->location_.z = static_cast<float>(loc_obj["z"].as_double());
                        loaded_char->location_.map_id = static_cast<uint32_t>(loc_obj["map_id"].as_int64());
                    }
                    
                    Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                        "Character loaded: " + std::to_string(character_id));
                    
                    return {loaded_char, std::nullopt};
                }
                else
                {
                    // Character not found in Redis, create new
                    return {std::make_shared<Character>(character_id, name), std::nullopt};
                }
            }
            catch (const std::exception& e)
            {
                return {std::make_shared<Character>(character_id, name), 
                       std::string("Exception during load: ") + e.what()};
            }
        }
        
        uint64_t id; // Public id member
        
    private:
        uint64_t character_id_;
        std::string name_;
        Location location_;
    };
}
