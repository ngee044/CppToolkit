#include "Character.h"

#include <cstring>

namespace GameNetwork
{
    Character::Character(uint64_t character_id, const std::string& name)
        : character_id_(character_id)
        , name_(name)
        , level_(1)
        , health_(100)
        , max_health_(100)
        , mana_(50)
        , max_mana_(50)
        , experience_(0)
    {
        location_ = {0.0f, 0.0f, 0.0f, 0, 0};
        created_time_ = std::chrono::system_clock::now();
        last_save_time_ = std::chrono::system_clock::now();
    }
    
    auto Character::character_id() const -> uint64_t
    {
        return character_id_;
    }
    
    auto Character::name() const -> const std::string&
    {
        return name_;
    }
    
    auto Character::level() const -> uint32_t
    {
        return level_;
    }
    
    auto Character::set_level(uint32_t level) -> void
    {
        level_ = level;
    }
    
    auto Character::location() const -> const Location&
    {
        return location_;
    }
    
    auto Character::set_location(const Location& location) -> void
    {
        location_ = location;
    }
    
    auto Character::health() const -> int32_t
    {
        return health_;
    }
    
    auto Character::max_health() const -> int32_t
    {
        return max_health_;
    }
    
    auto Character::set_health(int32_t health) -> void
    {
        health_ = std::max(0, std::min(health, max_health_));
    }
    
    auto Character::set_max_health(int32_t max_health) -> void
    {
        max_health_ = std::max(1, max_health);
        health_ = std::min(health_, max_health_);
    }
    
    auto Character::mana() const -> int32_t
    {
        return mana_;
    }
    
    auto Character::max_mana() const -> int32_t
    {
        return max_mana_;
    }
    
    auto Character::set_mana(int32_t mana) -> void
    {
        mana_ = std::max(0, std::min(mana, max_mana_));
    }
    
    auto Character::set_max_mana(int32_t max_mana) -> void
    {
        max_mana_ = std::max(1, max_mana);
        mana_ = std::min(mana_, max_mana_);
    }
    
    auto Character::experience() const -> uint64_t
    {
        return experience_;
    }
    
    auto Character::set_experience(uint64_t exp) -> void
    {
        experience_ = exp;
    }
    
    auto Character::add_experience(uint64_t exp) -> void
    {
        experience_ += exp;
    }
    
    auto Character::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> data;
        
        // Simple binary serialization
        // In production, use protobuf or similar
        
        // Character ID
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&character_id_),
                    reinterpret_cast<const uint8_t*>(&character_id_) + sizeof(uint64_t));
        
        // Name
        uint32_t name_len = static_cast<uint32_t>(name_.size());
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&name_len),
                    reinterpret_cast<const uint8_t*>(&name_len) + sizeof(uint32_t));
        data.insert(data.end(), name_.begin(), name_.end());
        
        // Level
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&level_),
                    reinterpret_cast<const uint8_t*>(&level_) + sizeof(uint32_t));
        
        // Location
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&location_),
                    reinterpret_cast<const uint8_t*>(&location_) + sizeof(Location));
        
        // Stats
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&health_),
                    reinterpret_cast<const uint8_t*>(&health_) + sizeof(int32_t));
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&max_health_),
                    reinterpret_cast<const uint8_t*>(&max_health_) + sizeof(int32_t));
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&mana_),
                    reinterpret_cast<const uint8_t*>(&mana_) + sizeof(int32_t));
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&max_mana_),
                    reinterpret_cast<const uint8_t*>(&max_mana_) + sizeof(int32_t));
        
        // Experience
        data.insert(data.end(), 
                    reinterpret_cast<const uint8_t*>(&experience_),
                    reinterpret_cast<const uint8_t*>(&experience_) + sizeof(uint64_t));
        
        return data;
    }
    
    auto Character::deserialize(const std::vector<uint8_t>& data) -> std::unique_ptr<Character>
    {
        if (data.size() < sizeof(uint64_t) + sizeof(uint32_t))
        {
            return nullptr;
        }
        
        size_t offset = 0;
        
        // Character ID
        uint64_t character_id;
        std::memcpy(&character_id, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        
        // Name
        uint32_t name_len;
        std::memcpy(&name_len, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        std::string name(reinterpret_cast<const char*>(data.data() + offset), name_len);
        offset += name_len;
        
        auto character = std::make_unique<Character>(character_id, name);
        
        // Level
        std::memcpy(&character->level_, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Location
        std::memcpy(&character->location_, data.data() + offset, sizeof(Location));
        offset += sizeof(Location);
        
        // Stats
        std::memcpy(&character->health_, data.data() + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        std::memcpy(&character->max_health_, data.data() + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        std::memcpy(&character->mana_, data.data() + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        std::memcpy(&character->max_mana_, data.data() + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        
        // Experience
        std::memcpy(&character->experience_, data.data() + offset, sizeof(uint64_t));
        
        return character;
    }
}