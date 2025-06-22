#include "MessageDispatcher.h"
#include "../Session/GameSession.h"
#include "GamePacket.h"

#include <chrono>
#include <thread>

namespace GameNetwork
{
    MessageDispatcher::MessageDispatcher()
        : rate_limiting_enabled_(false)
        , processing_running_(false)
    {
        stats_ = {};
    }
    
    MessageDispatcher::~MessageDispatcher()
    {
        if (processing_running_)
        {
            processing_running_ = false;
            if (processing_thread_.valid())
            {
                processing_thread_.wait();
            }
        }
    }
    
    auto MessageDispatcher::register_handler(PacketType type, PacketHandler handler) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[type] = handler;
    }
    
    auto MessageDispatcher::unregister_handler(PacketType type) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(type);
    }
    
    auto MessageDispatcher::has_handler(PacketType type) const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.find(type) != handlers_.end();
    }
    
    auto MessageDispatcher::dispatch(std::shared_ptr<GameSession> session, 
                                     std::unique_ptr<GamePacket> packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session || !packet)
        {
            return {false, "Invalid session or packet"};
        }
        
        auto packet_type = packet->type();
        
        // Check rate limiting
        if (rate_limiting_enabled_ && !check_rate_limit(session->session_id(), packet_type))
        {
            stats_.rate_limited++;
            return {false, "Rate limit exceeded"};
        }
        
        // Find handler
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(packet_type);
        if (it == handlers_.end())
        {
            stats_.failed_dispatches++;
            return {false, "No handler registered for packet type"};
        }
        
        // Execute handler
        auto start_time = std::chrono::steady_clock::now();
        auto [success, error] = it->second(session, *packet);
        auto end_time = std::chrono::steady_clock::now();
        
        // Update statistics
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        
        stats_.total_dispatched++;
        stats_.dispatch_count_by_type[packet_type]++;
        
        if (stats_.average_processing_time_us.find(packet_type) == 
            stats_.average_processing_time_us.end())
        {
            stats_.average_processing_time_us[packet_type] = duration.count();
        }
        else
        {
            // Update rolling average
            auto& avg = stats_.average_processing_time_us[packet_type];
            avg = (avg * 9 + duration.count()) / 10;
        }
        
        if (!success)
        {
            stats_.failed_dispatches++;
        }
        
        return {success, error};
    }
    
    auto MessageDispatcher::dispatch_async(std::shared_ptr<GameSession> session, 
                                           std::unique_ptr<GamePacket> packet) -> void
    {
        if (!session || !packet)
        {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        QueuedMessage msg;
        msg.session = session;
        msg.packet = std::move(packet);
        msg.priority = get_priority(msg.packet->type());
        msg.queued_time = std::chrono::steady_clock::now();
        
        message_queue_.push(std::move(msg));
        
        // Start processing thread if not running
        if (!processing_running_)
        {
            processing_running_ = true;
            processing_thread_ = std::async(std::launch::async, [this]()
            {
                process_queue();
            });
        }
    }
    
    auto MessageDispatcher::set_priority(PacketType type, PacketPriority priority) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        priorities_[type] = priority;
    }
    
    auto MessageDispatcher::get_priority(PacketType type) const -> PacketPriority
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = priorities_.find(type);
        if (it != priorities_.end())
        {
            return it->second;
        }
        
        // Default priorities based on packet type
        switch (type)
        {
            case PacketType::Heartbeat:
                return PacketPriority::Critical;
            case PacketType::Authentication:
            case PacketType::Disconnect:
                return PacketPriority::High;
            case PacketType::MoveTo:
            case PacketType::Attack:
                return PacketPriority::Normal;
            case PacketType::ChatMessage:
                return PacketPriority::Low;
            default:
                return PacketPriority::Normal;
        }
    }
    
    auto MessageDispatcher::enable_rate_limiting(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_limiting_enabled_ = enable;
    }
    
    auto MessageDispatcher::is_rate_limiting_enabled() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return rate_limiting_enabled_;
    }
    
    auto MessageDispatcher::set_rate_limit(PacketType type, uint32_t max_per_second) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rate_limits_[type] = max_per_second;
    }
    
    auto MessageDispatcher::check_rate_limit(const std::string& session_id, PacketType type) -> bool
    {
        if (!rate_limiting_enabled_)
        {
            return true;
        }
        
        auto now = std::chrono::steady_clock::now();
        
        // Get or create rate limit info
        auto& session_limits = rate_limit_tracking_[session_id];
        auto& limit_info = session_limits[type];
        
        // Get limit for this packet type
        uint32_t max_per_second = 10;  // Default
        auto limit_it = rate_limits_.find(type);
        if (limit_it != rate_limits_.end())
        {
            max_per_second = limit_it->second;
        }
        
        // Check if window needs reset
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - limit_info.window_start);
        
        if (elapsed.count() >= 1)
        {
            // Reset window
            limit_info.window_start = now;
            limit_info.count_in_window = 0;
        }
        
        // Check limit
        if (limit_info.count_in_window >= max_per_second)
        {
            return false;
        }
        
        limit_info.count_in_window++;
        return true;
    }
    
    auto MessageDispatcher::register_default_handlers() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Register heartbeat handler
        register_handler(PacketType::Heartbeat, 
            [](std::shared_ptr<GameSession> session, const GamePacket& packet) 
            -> std::tuple<bool, std::optional<std::string>>
            {
                // Update session activity
                session->update_last_activity();
                
                // Echo heartbeat back
                auto heartbeat = std::make_unique<HeartbeatPacket>();
                heartbeat->set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                
                if (session->connection())
                {
                    session->connection()->send_packet(*heartbeat);
                }
                
                return {true, std::nullopt};
            });
            
        // Register disconnect handler
        register_handler(PacketType::Disconnect,
            [](std::shared_ptr<GameSession> session, const GamePacket& packet)
            -> std::tuple<bool, std::optional<std::string>>
            {
                // Handle graceful disconnect
                session->set_state(SessionState::Terminating);
                return {true, std::nullopt};
            });
    }
    
    auto MessageDispatcher::process_queue() -> void
    {
        while (processing_running_)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Wait for messages or timeout
            if (message_queue_.empty())
            {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Get highest priority message
            auto queued_msg = std::move(const_cast<QueuedMessage&>(message_queue_.top()));
            message_queue_.pop();
            
            lock.unlock();
            
            // Find handler
            auto handler_it = handlers_.find(queued_msg.packet->type());
            if (handler_it != handlers_.end())
            {
                auto start_time = std::chrono::steady_clock::now();
                
                // Execute handler
                auto [success, error] = handler_it->second(queued_msg.session, *queued_msg.packet);
                
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start_time);
                    
                // Update statistics
                lock.lock();
                stats_.total_dispatched++;
                stats_.dispatch_count_by_type[queued_msg.packet->type()]++;
                stats_.average_processing_time_us[queued_msg.packet->type()] = 
                    (stats_.average_processing_time_us[queued_msg.packet->type()] + duration.count()) / 2;
                
                if (!success)
                {
                    stats_.failed_dispatches++;
                }
                lock.unlock();
            }
            else
            {
                lock.lock();
                stats_.failed_dispatches++;
                lock.unlock();
            }
        }
    }}