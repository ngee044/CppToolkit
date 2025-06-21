#pragma once

#include "../Packet/GamePacket.h"

#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <optional>
#include <tuple>
#include <future>
#include <atomic>

namespace GameNetwork
{
    class GameSession;
    
    using PacketHandler = std::function<std::tuple<bool, std::optional<std::string>>(
        std::shared_ptr<GameSession>, const GamePacket&)>;
    
    class MessageDispatcher : public std::enable_shared_from_this<MessageDispatcher>
    {
    public:
        MessageDispatcher();
        virtual ~MessageDispatcher();
        
        // Handler registration
        auto register_handler(PacketType type, PacketHandler handler) -> void;
        auto unregister_handler(PacketType type) -> void;
        auto has_handler(PacketType type) const -> bool;
        
        // Message dispatching
        auto dispatch(std::shared_ptr<GameSession> session, 
                      std::unique_ptr<GamePacket> packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Async dispatching
        auto dispatch_async(std::shared_ptr<GameSession> session, 
                            std::unique_ptr<GamePacket> packet) -> void;
        
        // Priority queue management
        auto set_priority(PacketType type, PacketPriority priority) -> void;
        auto get_priority(PacketType type) const -> PacketPriority;
        
        // Rate limiting
        auto enable_rate_limiting(bool enable) -> void;
        auto is_rate_limiting_enabled() const -> bool;
        auto set_rate_limit(PacketType type, uint32_t max_per_second) -> void;
        auto check_rate_limit(const std::string& session_id, PacketType type) -> bool;
        
        // Default handlers
        auto register_default_handlers() -> void;
        
        // Statistics
        struct DispatcherStats
        {
            uint64_t total_dispatched;
            uint64_t failed_dispatches;
            uint64_t rate_limited;
            std::unordered_map<PacketType, uint64_t> dispatch_count_by_type;
            std::unordered_map<PacketType, uint64_t> average_processing_time_us;
        };
        
        auto get_stats() const -> DispatcherStats;
        auto reset_stats() -> void;
        
    private:
        struct QueuedMessage
        {
            std::shared_ptr<GameSession> session;
            std::unique_ptr<GamePacket> packet;
            PacketPriority priority;
            std::chrono::steady_clock::time_point queued_time;
            
            auto operator<(const QueuedMessage& other) const -> bool
            {
                return priority < other.priority;
            }
        };
        
        struct RateLimitInfo
        {
            uint32_t max_per_second;
            std::chrono::steady_clock::time_point window_start;
            uint32_t count_in_window;
        };
        
        auto process_queue() -> void;
        auto handle_heartbeat(std::shared_ptr<GameSession> session, 
                              const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        auto handle_authentication(std::shared_ptr<GameSession> session, 
                                   const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>;
        
    private:
        mutable std::mutex mutex_;
        
        // Handler storage
        std::unordered_map<PacketType, PacketHandler> handlers_;
        std::unordered_map<PacketType, PacketPriority> priorities_;
        
        // Message queue
        std::priority_queue<QueuedMessage> message_queue_;
        
        // Rate limiting
        bool rate_limiting_enabled_;
        std::unordered_map<PacketType, uint32_t> rate_limits_;
        std::unordered_map<std::string, std::unordered_map<PacketType, RateLimitInfo>> rate_limit_tracking_;
        
        // Statistics
        DispatcherStats stats_;
        
        // Processing thread
        std::future<void> processing_thread_;
        std::atomic<bool> processing_running_;
    };
}
