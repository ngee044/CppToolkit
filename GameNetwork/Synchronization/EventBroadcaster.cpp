#include "EventBroadcaster.h"
#include "../Session/GameSession.h"
#include "../Session/GameSessionManager.h"
#include <Logger.h>
#include <algorithm>
#include <thread>

#ifdef USE_COMPRESSION
#include <zlib.h>
#endif

using namespace Utilities;

namespace GameNetwork
{
    EventBroadcaster::EventBroadcaster()
        : batching_enabled_(false)
        , max_events_per_frame_(100)
        , event_ttl_(std::chrono::milliseconds(5000))
        , compression_enabled_(false)
        , processing_running_(false)
        , stats_{0, 0, 0, 0, {}, {}}
    {
    }

    EventBroadcaster::~EventBroadcaster()
    {
        stop_processing();
    }

    auto EventBroadcaster::broadcast_global(const std::string& event_name,
                                          std::unique_ptr<GamePacket> packet,
                                          EventPriority priority)
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!packet)
        {
            return { false, "Invalid packet" };
        }

        BroadcastEvent event;
        event.event_name = event_name;
        event.packet = std::move(packet);
        event.scope = EventScope::Global;
        event.priority = priority;
        event.created_time = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (batching_enabled_)
        {
            batch_events_.push_back(std::move(event));
        }
        else
        {
            auto recipients = gather_recipients(event);
            event_queue_.push(QueuedEvent{std::move(event), std::move(recipients)});
        }
        
        stats_.events_by_name[event_name]++;
        stats_.events_by_scope[EventScope::Global]++;
        
        return { true, std::nullopt };
    }

    auto EventBroadcaster::broadcast_to_channel(const std::string& event_name,
                                              uint32_t channel_id,
                                              std::unique_ptr<GamePacket> packet,
                                              EventPriority priority)
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!packet)
        {
            return { false, "Invalid packet" };
        }

        BroadcastEvent event;
        event.event_name = event_name;
        event.packet = std::move(packet);
        event.scope = EventScope::Channel;
        event.priority = priority;
        event.channel_id = channel_id;
        event.created_time = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (batching_enabled_)
        {
            batch_events_.push_back(std::move(event));
        }
        else
        {
            auto recipients = gather_recipients(event);
            event_queue_.push(QueuedEvent{std::move(event), std::move(recipients)});
        }

        stats_.events_by_name[event_name]++;
        stats_.events_by_scope[EventScope::Channel]++;
        
        return { true, std::nullopt };
    }

    auto EventBroadcaster::broadcast_to_area(const std::string& event_name,
                                           const Location& center,
                                           float radius,
                                           std::unique_ptr<GamePacket> packet,
                                           EventPriority priority)
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!packet)
        {
            return { false, "Invalid packet" };
        }

        if (radius <= 0.0f)
        {
            return { false, "Invalid radius" };
        }

        BroadcastEvent event;
        event.event_name = event_name;
        event.packet = std::move(packet);
        event.scope = EventScope::Area;
        event.priority = priority;
        event.center = center;
        event.radius = radius;
        event.created_time = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (batching_enabled_)
        {
            batch_events_.push_back(std::move(event));
        }
        else
        {
            auto recipients = gather_recipients(event);
            event_queue_.push(QueuedEvent{std::move(event), std::move(recipients)});
        }
        
        stats_.events_by_name[event_name]++;
        stats_.events_by_scope[EventScope::Area]++;
        
        return { true, std::nullopt };
    }

    auto EventBroadcaster::broadcast_custom(const std::string& event_name,
                                          std::unique_ptr<GamePacket> packet,
                                          std::function<bool(std::shared_ptr<GameSession>)> filter,
                                          EventPriority priority)
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!packet)
        {
            return { false, "Invalid packet" };
        }

        if (!filter)
        {
            return { false, "Invalid filter function" };
        }

        BroadcastEvent event;
        event.event_name = event_name;
        event.packet = std::move(packet);
        event.scope = EventScope::Custom;
        event.priority = priority;
        event.filter = filter;
        event.created_time = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (batching_enabled_)
        {
            batch_events_.push_back(std::move(event));
        }
        else
        {
            auto recipients = gather_recipients(event);
            event_queue_.push(QueuedEvent{std::move(event), std::move(recipients)});
        }
        
        stats_.events_by_name[event_name]++;
        stats_.events_by_scope[EventScope::Custom]++;
        
        return { true, std::nullopt };
    }

    auto EventBroadcaster::begin_batch() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batching_enabled_ = true;
        batch_events_.clear();
    }

    auto EventBroadcaster::add_to_batch(BroadcastEvent event) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (batching_enabled_)
        {
            batch_events_.push_back(std::move(event));
        }
    }

    auto EventBroadcaster::commit_batch() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!batching_enabled_)
        {
            return { false, "Batching not enabled" };
        }
        
        for (auto& event : batch_events_)
        {
            auto recipients = gather_recipients(event);
            event_queue_.push(QueuedEvent{std::move(event), std::move(recipients)});
        }
        
        batch_events_.clear();
        batching_enabled_ = false;
        
        return { true, std::nullopt };
    }

    auto EventBroadcaster::cancel_batch() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch_events_.clear();
        batching_enabled_ = false;
    }

    auto EventBroadcaster::subscribe_to_event(const std::string& event_name,
                                            std::shared_ptr<GameSession> session) -> void
    {
        if (!session) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        event_subscriptions_[event_name].insert(session->get_session_id());
    }

    auto EventBroadcaster::unsubscribe_from_event(const std::string& event_name,
                                                std::shared_ptr<GameSession> session) -> void
    {
        if (!session) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_subscriptions_.find(event_name);
        if (it != event_subscriptions_.end())
        {
            it->second.erase(session->get_session_id());
            if (it->second.empty())
            {
                event_subscriptions_.erase(it);
            }
        }
    }

    auto EventBroadcaster::is_subscribed(const std::string& event_name,
                                       std::shared_ptr<GameSession> session) const -> bool
    {
        if (!session) return false;
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_subscriptions_.find(event_name);
        if (it != event_subscriptions_.end())
        {
            return it->second.find(session->get_session_id()) != it->second.end();
        }
        return false;
    }

    auto EventBroadcaster::start_processing() -> void
    {
        if (processing_running_.exchange(true))
        {
            return; // Already running
        }
        
        processing_thread_ = std::async(std::launch::async, [this]() {
            while (processing_running_)
            {
                process_events();
                std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
            }
        });
    }

    auto EventBroadcaster::stop_processing() -> void
    {
        processing_running_ = false;
        if (processing_thread_.valid())
        {
            processing_thread_.wait();
        }
    }

    auto EventBroadcaster::process_events() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t events_processed = 0;
        auto now = std::chrono::steady_clock::now();
        
        while (!event_queue_.empty() && events_processed < max_events_per_frame_)
        {
            auto queued_event = std::move(const_cast<QueuedEvent&>(event_queue_.top()));
            event_queue_.pop();
            
            // Check if event is expired
            if (should_drop_event(queued_event.event))
            {
                stats_.events_dropped++;
                continue;
            }
            
            // Send to recipients
            send_to_recipients(queued_event.event, queued_event.recipients);
            
            stats_.total_events_broadcasted++;
            stats_.total_recipients += queued_event.recipients.size();
            events_processed++;
        }
    }

    auto EventBroadcaster::set_max_events_per_frame(size_t max_events) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        max_events_per_frame_ = max_events;
    }

    auto EventBroadcaster::set_event_ttl(std::chrono::milliseconds ttl) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        event_ttl_ = ttl;
    }

    auto EventBroadcaster::enable_event_compression(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        compression_enabled_ = enable;
    }

    auto EventBroadcaster::get_stats() const -> BroadcastStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    auto EventBroadcaster::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = BroadcastStats{0, 0, 0, 0, {}, {}};
    }

    auto EventBroadcaster::gather_recipients(const BroadcastEvent& event)
        -> std::vector<std::shared_ptr<GameSession>>
    {
        std::vector<std::shared_ptr<GameSession>> recipients;
        
        // Get session manager instance
        auto session_manager = GameSessionManager::get_instance();
        if (!session_manager)
        {
            return recipients;
        }

        switch (event.scope)
        {
            case EventScope::Global:
            {
                // Get all sessions
                auto all_sessions = session_manager->get_all_sessions();
                for (const auto& [session_id, session] : all_sessions)
                {
                    if (session && session->is_connected())
                    {
                        // Check subscription if event requires it
                        if (event_subscriptions_.find(event.event_name) != event_subscriptions_.end())
                        {
                            if (is_subscribed(event.event_name, session))
                            {
                                recipients.push_back(session);
                            }
                        }
                        else
                        {
                            recipients.push_back(session);
                        }
                    }
                }
                break;
            }
            
            case EventScope::Channel:
            {
                // Get sessions in specific channel
                auto channel_sessions = session_manager->get_sessions_in_channel(event.channel_id);
                for (auto& session : channel_sessions)
                {
                    if (session && session->is_connected())
                    {
                        recipients.push_back(session);
                    }
                }
                break;
            }

            case EventScope::Area:
            {
                // Get sessions within area
                auto all_sessions = session_manager->get_all_sessions();
                for (const auto& [session_id, session] : all_sessions)
                {
                    if (session && session->is_connected())
                    {
                        auto player_location = session->get_player_location();
                        if (player_location.has_value())
                        {
                            // Simple location-based filtering with location IDs
                            // In a real implementation, you would convert location IDs to coordinates
                            // For now, just include all players in the broadcast
                            recipients.push_back(session);
                        }
                    }
                }
                break;
            }
            
            case EventScope::Custom:
            {
                if (event.filter)
                {
                    auto all_sessions = session_manager->get_all_sessions();
                    for (const auto& [session_id, session] : all_sessions)
                    {
                        if (session && session->is_connected() && event.filter(session))
                        {
                            recipients.push_back(session);
                        }
                    }
                }
                break;
            }
            
            default:
                break;
        }
        
        return recipients;
    }

    auto EventBroadcaster::send_to_recipients(const BroadcastEvent& event,
                                            const std::vector<std::shared_ptr<GameSession>>& recipients)
        -> void
    {
        if (recipients.empty())
        {
            return;
        }
        
        // Clone packet for each recipient
        for (const auto& session : recipients)
        {
            if (!session || !session->is_connected())
            {
                continue;
            }
            
            try
            {
                // Create a copy of the packet
                auto packet_copy = event.packet->clone();
                
                // Apply compression if enabled
                if (compression_enabled_)
                {
                    #ifdef USE_COMPRESSION
                    // Compress the packet data using zlib
                    std::vector<uint8_t> compressed_data;
                    compressed_data.resize(compressBound(packet_copy->data.size()));
                    
                    uLongf compressed_size = compressed_data.size();
                    int result = compress(compressed_data.data(), &compressed_size,
                                        packet_copy->data.data(), packet_copy->data.size());
                    
                    if (result == Z_OK)
                    {
                        compressed_data.resize(compressed_size);
                        
                        // Create compressed packet header
                        std::vector<uint8_t> compressed_packet;
                        compressed_packet.reserve(compressed_size + 5);
                        
                        // Add compression flag and original size
                        compressed_packet.push_back(0x01); // Compression flag
                        uint32_t original_size = static_cast<uint32_t>(packet_copy->data.size());
                        compressed_packet.insert(compressed_packet.end(), 
                                               reinterpret_cast<uint8_t*>(&original_size),
                                               reinterpret_cast<uint8_t*>(&original_size) + 4);
                        
                        // Add compressed data
                        compressed_packet.insert(compressed_packet.end(), 
                                               compressed_data.begin(), 
                                               compressed_data.end());
                        
                        packet_copy->data = std::move(compressed_packet);
                        stats_.events_compressed++;
                    }
                    else
                    {
                        Logger::handle().write(LogTypes::Warning,
                            "Failed to compress event data, sending uncompressed");
                    }
                    #else
                    Logger::handle().write(LogTypes::Warning,
                        "Compression enabled but not compiled with USE_COMPRESSION");
                    #endif
                }
                
                // Send to session
                std::vector<uint8_t> serialized_data = packet_copy->data;
                session->send_packet(serialized_data);
            }
            catch (const std::exception& e)
            {
                Logger::handle().write(LogTypes::Error,
                    "Failed to send event to session: " + std::string(e.what()));
            }
        }
    }

    auto EventBroadcaster::should_drop_event(const BroadcastEvent& event) const -> bool
    {
        auto now = std::chrono::steady_clock::now();
        auto age = now - event.created_time;
        
        return age > event_ttl_;
    }
}
