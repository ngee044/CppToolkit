#pragma once

#include "../Packet/GamePacket.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <optional>
#include <tuple>
#include <regex>
#include <deque>
#include <mutex>

namespace GameNetwork
{
    enum class ValidationResult
    {
        Valid = 0,
        InvalidSize = 1,
        InvalidType = 2,
        InvalidSequence = 3,
        InvalidChecksum = 4,
        InvalidTimestamp = 5,
        InvalidContent = 6,
        Suspicious = 7,
        Blocked = 8
    };
    
    struct ValidationRule
    {
        std::string name;
        std::function<ValidationResult(const GamePacket&)> validator;
        bool enabled;
        uint32_t priority;
    };
    
    struct SuspiciousActivity
    {
        std::string session_id;
        std::string reason;
        std::chrono::steady_clock::time_point timestamp;
        PacketType packet_type;
        std::vector<uint8_t> packet_data;
    };
    
    class GamePacketValidator : public std::enable_shared_from_this<GamePacketValidator>
    {
    public:
        GamePacketValidator();
        virtual ~GamePacketValidator() = default;
        
        // Basic validation
        auto validate_packet(const GamePacket& packet, 
                             const std::string& session_id) 
            -> std::tuple<ValidationResult, std::optional<std::string>>;
        
        // Size validation
        auto validate_packet_size(const GamePacket& packet) const -> ValidationResult;
        auto set_max_packet_size(PacketType type, size_t max_size) -> void;
        auto set_global_max_packet_size(size_t max_size) -> void;
        
        // Type validation
        auto is_packet_type_allowed(PacketType type) const -> bool;
        auto allow_packet_type(PacketType type) -> void;
        auto block_packet_type(PacketType type) -> void;
        
        // Sequence validation
        auto validate_sequence(const std::string& session_id, 
                               uint32_t sequence) -> ValidationResult;
        auto reset_sequence(const std::string& session_id) -> void;
        
        // Content validation
        auto add_content_validator(const std::string& name,
                                   std::function<ValidationResult(const GamePacket&)> validator,
                                   uint32_t priority = 100) -> void;
        auto remove_content_validator(const std::string& name) -> void;
        
        // Anti-cheat validation
        auto validate_movement(const Location& from, 
                               const Location& to, 
                               float delta_time) const -> ValidationResult;
        auto validate_action_timing(const std::string& session_id,
                                    const std::string& action,
                                    std::chrono::steady_clock::time_point timestamp) 
            -> ValidationResult;
        auto validate_resource_change(int32_t current, 
                                      int32_t change, 
                                      int32_t max) const -> ValidationResult;
        
        // Pattern detection
        auto add_pattern_rule(const std::string& pattern_name,
                              const std::regex& pattern,
                              ValidationResult result) -> void;
        auto check_patterns(const std::vector<uint8_t>& data) const -> ValidationResult;
        
        // Session tracking
        auto track_session_behavior(const std::string& session_id,
                                    const GamePacket& packet) -> void;
        auto get_session_trust_score(const std::string& session_id) const -> float;
        auto is_session_suspicious(const std::string& session_id) const -> bool;
        
        // Blacklist/Whitelist
        auto add_to_blacklist(const std::string& session_id, 
                              const std::string& reason) -> void;
        auto remove_from_blacklist(const std::string& session_id) -> void;
        auto is_blacklisted(const std::string& session_id) const -> bool;
        
        // Suspicious activity logging
        auto get_suspicious_activities(size_t max_count = 100) const 
            -> std::vector<SuspiciousActivity>;
        auto clear_suspicious_activities() -> void;
        
        // Configuration
        auto enable_strict_mode(bool enable) -> void;
        auto set_validation_level(uint8_t level) -> void;  // 0-10, higher is stricter
        auto enable_logging(bool enable) -> void;
        
        // Statistics
        struct ValidationStats
        {
            uint64_t total_validations;
            uint64_t passed_validations;
            std::unordered_map<ValidationResult, uint64_t> failures_by_type;
            std::unordered_map<PacketType, uint64_t> validations_by_packet_type;
            uint64_t suspicious_activities_detected;
            uint64_t blacklisted_attempts;
        };
        
        auto get_stats() const -> ValidationStats;
        auto reset_stats() -> void;
        
    private:
        struct SessionTrackingData
        {
            uint32_t last_sequence;
            std::chrono::steady_clock::time_point last_packet_time;
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_actions;
            uint32_t suspicious_count;
            float trust_score;
            std::deque<float> packet_rate_history;
        };
        
        struct PatternRule
        {
            std::string name;
            std::regex pattern;
            ValidationResult result;
        };
        
        auto log_suspicious_activity(const std::string& session_id,
                                     const std::string& reason,
                                     const GamePacket& packet) -> void;
        auto update_trust_score(const std::string& session_id,
                                float adjustment) -> void;
        auto calculate_packet_rate(const std::string& session_id) const -> float;
        
    private:
        mutable std::mutex mutex_;
        
        // Validation rules
        std::vector<ValidationRule> validation_rules_;
        std::unordered_map<PacketType, size_t> max_packet_sizes_;
        size_t global_max_packet_size_;
        
        // Allowed/Blocked types
        std::unordered_set<PacketType> allowed_packet_types_;
        std::unordered_set<PacketType> blocked_packet_types_;
        
        // Pattern rules
        std::vector<PatternRule> pattern_rules_;
        
        // Session tracking
        std::unordered_map<std::string, SessionTrackingData> session_data_;
        
        // Blacklist
        std::unordered_set<std::string> blacklisted_sessions_;
        
        // Suspicious activities
        std::deque<SuspiciousActivity> suspicious_activities_;
        
        // Configuration
        bool strict_mode_;
        uint8_t validation_level_;
        bool logging_enabled_;
        
        // Statistics
        ValidationStats stats_;
        
        // Constants
        static constexpr size_t DEFAULT_MAX_PACKET_SIZE = 65536;
        static constexpr float INITIAL_TRUST_SCORE = 100.0f;
        static constexpr size_t MAX_SUSPICIOUS_ACTIVITIES = 1000;
        static constexpr float MAX_MOVEMENT_SPEED = 10.0f; // Units per second
    };
}
