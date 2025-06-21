#pragma once

#include "../GameNetworkConstants.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <any>
#include <memory>
#include <optional>
#include <vector>

namespace GameNetwork
{
    class Character
    {
    public:
        Character(uint64_t character_id, const std::string& name);
        virtual ~Character() = default;
        
        // Basic info
        auto character_id() const -> uint64_t;
        auto name() const -> const std::string&;
        auto level() const -> uint32_t;
        auto set_level(uint32_t level) -> void;
        
        // Location
        auto location() const -> const Location&;
        auto set_location(const Location& location) -> void;
        
        // Stats
        auto health() const -> int32_t;
        auto max_health() const -> int32_t;
        auto mana() const -> int32_t;
        auto max_mana() const -> int32_t;
        
        auto set_health(int32_t health) -> void;
        auto set_max_health(int32_t max_health) -> void;
        auto set_mana(int32_t mana) -> void;
        auto set_max_mana(int32_t max_mana) -> void;
        
        // Experience
        auto experience() const -> uint64_t;
        auto set_experience(uint64_t exp) -> void;
        auto add_experience(uint64_t exp) -> void;
        
        // Custom properties
        template<typename T>
        auto set_property(const std::string& key, const T& value) -> void
        {
            properties_[key] = value;
        }
        
        template<typename T>
        auto get_property(const std::string& key) const -> std::optional<T>
        {
            auto it = properties_.find(key);
            if (it != properties_.end())
            {
                try
                {
                    return std::any_cast<T>(it->second);
                }
                catch (const std::bad_any_cast&)
                {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }
        
        // Serialization
        auto serialize() const -> std::vector<uint8_t>;
        static auto deserialize(const std::vector<uint8_t>& data) 
            -> std::unique_ptr<Character>;
        
    private:
        uint64_t character_id_;
        std::string name_;
        uint32_t level_;
        
        Location location_;
        
        // Stats
        int32_t health_;
        int32_t max_health_;
        int32_t mana_;
        int32_t max_mana_;
        
        uint64_t experience_;
        
        // Custom properties
        std::unordered_map<std::string, std::any> properties_;
        
        // Timestamps
        std::chrono::system_clock::time_point created_time_;
        std::chrono::system_clock::time_point last_save_time_;
    };
}
