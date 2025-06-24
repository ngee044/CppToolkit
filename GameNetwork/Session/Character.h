#pragma once

#include "../GameNetworkConstants.h"
#include <string>
#include <optional>
#include <tuple>
#include <chrono>

namespace GameNetwork
{
    class Character
    {
    public:
        Character();
        ~Character();
        
        // Character data
        auto id() const -> uint64_t { return character_id_; }
        auto name() const -> const std::string& { return name_; }
        auto level() const -> uint32_t { return level_; }
        auto experience() const -> uint64_t { return experience_; }
        
        // Location
        auto get_location() const -> Location { return location_; }
        auto set_location(const Location& loc) -> void { location_ = loc; }
        
        // Load/Save
        auto load(uint64_t character_id, const std::string& account_id) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto save() -> std::tuple<bool, std::optional<std::string>>;
        
        // Stats
        auto get_health() const -> uint32_t { return health_; }
        auto get_max_health() const -> uint32_t { return max_health_; }
        auto get_mana() const -> uint32_t { return mana_; }
        auto get_max_mana() const -> uint32_t { return max_mana_; }
        
        auto set_health(uint32_t value) -> void { health_ = std::min(value, max_health_); }
        auto set_mana(uint32_t value) -> void { mana_ = std::min(value, max_mana_); }
        
    private:
        uint64_t character_id_;
        std::string account_id_;
        std::string name_;
        uint32_t level_;
        uint64_t experience_;
        
        Location location_;
        
        uint32_t health_;
        uint32_t max_health_;
        uint32_t mana_;
        uint32_t max_mana_;
        
        std::chrono::system_clock::time_point last_save_time_;
    };
}
