#pragma once

#include "../GameNetworkConstants.h"
#include "../Packet/GamePacket.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>
#include <mutex>
#include <optional>
#include <tuple>
#include <future>
#include <atomic>

namespace GameNetwork
{
    class GameSession;
    
    enum class EventScope
    {
        Global = 0,      // All connected clients
        Channel = 1,     // Specific channel
        Area = 2,        // Area of interest
        Party = 3,       // Party members
        Guild = 4,       // Guild members
        Custom = 5       // Custom filter
    };
    
    enum class EventPriority
    {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };
    
    struct BroadcastEvent
    {
        std::string event_name;
        std::unique_ptr<GamePacket> packet;
        EventScope scope;
        EventPriority priority;
        std::chrono::steady_clock::time_point created_time;
        
        // Scope-specific data
        uint32_t channel_id;         // For Channel scope
        Location center;             // For Area scope
        float radius;                // For Area scope
        std::vector<std::string> target_sessions; // For Custom scope
        
        // Filtering
        std::function<bool(std::shared_ptr<GameSession>)> filter;
    };
    
    class EventBroadcaster : public std::enable_shared_from_this<EventBroadcaster>
    {
    public:
        EventBroadcaster();
        virtual ~EventBroadcaster();
        
        // Event broadcasting
        auto broadcast_global(const std::string& event_name,
                              std::unique_ptr<GamePacket> packet,
                              EventPriority priority = EventPriority::Normal) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        auto broadcast_to_channel(const std::string& event_name,
                                  uint32_t channel_id,
                                  std::unique_ptr<GamePacket> packet,
                                  EventPriority priority = EventPriority::Normal) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        auto broadcast_to_area(const std::string& event_name,
                               const Location& center,
                               float radius,
                               std::unique_ptr<GamePacket> packet,
                               EventPriority priority = EventPriority::Normal) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        auto broadcast_custom(const std::string& event_name,
                              std::unique_ptr<GamePacket> packet,
                              std::function<bool(std::shared_ptr<GameSession>)> filter,
                              EventPriority priority = EventPriority::Normal) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // Batch broadcasting
        auto begin_batch() -> void;
        auto add_to_batch(BroadcastEvent event) -> void;
        auto commit_batch() -> std::tuple<bool, std::optional<std::string>>;
        auto cancel_batch() -> void;
        
        // Event subscription (for filtering)
        auto subscribe_to_event(const std::string& event_name,
                                std::shared_ptr<GameSession> session) -> void;
        auto unsubscribe_from_event(const std::string& event_name,
                                    std::shared_ptr<GameSession> session) -> void;
        auto is_subscribed(const std::string& event_name,
                           std::shared_ptr<GameSession> session) const -> bool;
        
        // Processing
        auto start_processing() -> void;
        auto stop_processing() -> void;
        auto process_events() -> void;
        
        // Configuration
        auto set_max_events_per_frame(size_t max_events) -> void;
        auto set_event_ttl(std::chrono::milliseconds ttl) -> void;
        auto enable_event_compression(bool enable) -> void;
        
        // Statistics
        struct BroadcastStats
        {
            uint64_t total_events_broadcasted;
            uint64_t total_recipients;
            uint64_t events_compressed;
            uint64_t events_dropped;
            std::unordered_map<std::string, uint64_t> events_by_name;
            std::unordered_map<EventScope, uint64_t> events_by_scope;
        };
        
        auto get_stats() const -> BroadcastStats;
        auto reset_stats() -> void;
        
    private:
        struct QueuedEvent
        {
            BroadcastEvent event;
            std::vector<std::shared_ptr<GameSession>> recipients;
            
            auto operator<(const QueuedEvent& other) const -> bool
            {
                return event.priority < other.event.priority;
            }
        };
        
        auto gather_recipients(const BroadcastEvent& event) 
            -> std::vector<std::shared_ptr<GameSession>>;
        auto send_to_recipients(const BroadcastEvent& event,
                                const std::vector<std::shared_ptr<GameSession>>& recipients) 
            -> void;
        auto should_drop_event(const BroadcastEvent& event) const -> bool;
        
    private:
        mutable std::mutex mutex_;
        
        // Event queue
        std::priority_queue<QueuedEvent> event_queue_;
        std::vector<BroadcastEvent> batch_events_;
        bool batching_enabled_;
        
        // Event subscriptions
        std::unordered_map<std::string, std::unordered_set<std::string>> event_subscriptions_;
        
        // Configuration
        size_t max_events_per_frame_;
        std::chrono::milliseconds event_ttl_;
        bool compression_enabled_;
        
        // Processing
        std::future<void> processing_thread_;
        std::atomic<bool> processing_running_;
        
        // Statistics
        BroadcastStats stats_;
    };
}
