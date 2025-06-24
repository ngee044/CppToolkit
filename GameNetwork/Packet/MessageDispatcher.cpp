#include "MessageDispatcher.h"
#include <GameSession.h>
#include <Logger.h>
#include <ThreadPool.h>
#include <chrono>

namespace GameNetwork
{
    MessageDispatcher::MessageDispatcher()
        : rate_limiting_enabled_(false)
        , is_running_(false)
    {
        stats_.total_dispatched = 0;
        stats_.failed_dispatches = 0;
        stats_.rate_limited = 0;
    }

    MessageDispatcher::~MessageDispatcher()
    {
        stop();
    }

    auto MessageDispatcher::register_handler(PacketType type, PacketHandler handler) -> void
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[type] = handler;
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Registered handler for packet type: " + std::to_string(static_cast<uint16_t>(type)));
    }

    auto MessageDispatcher::unregister_handler(PacketType type) -> void
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_.erase(type);
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Unregistered handler for packet type: " + std::to_string(static_cast<uint16_t>(type)));
    }

    auto MessageDispatcher::has_handler(PacketType type) const -> bool
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        return handlers_.find(type) != handlers_.end();
    }

    auto MessageDispatcher::dispatch(std::shared_ptr<GameSession> session, const GamePacket& packet) 
        -> void
    {
        if (!session)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                "Dispatch called with null session");
            stats_.failed_dispatches++;
            return;
        }

        auto type = packet.get_type();
        
        // Check rate limiting
        if (rate_limiting_enabled_)
        {
            if (!check_rate_limit(session->session_id(), type))
            {
                stats_.rate_limited++;
                Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                    "Rate limit exceeded for session: " + session->session_id());
                return;
            }
        }

        // Find handler
        PacketHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            auto it = handlers_.find(type);
            if (it == handlers_.end())
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                    "No handler found for packet type: " + std::to_string(static_cast<uint16_t>(type)));
                stats_.failed_dispatches++;
                return;
            }
            handler = it->second;
        }
        
        // Record start time
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Execute handler
        try
        {
            auto [success, error] = handler(session, packet);
            
            if (!success)
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Warning,
                    "Handler failed for packet type " + std::to_string(static_cast<uint16_t>(type)) + 
                    ": " + error.value_or("Unknown error"));
                stats_.failed_dispatches++;
            }
            else
            {
                stats_.total_dispatched++;
                stats_.dispatch_count_by_type[type]++;
            }
        }
        catch (const std::exception& e)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Exception in packet handler: " + std::string(e.what()));
            stats_.failed_dispatches++;
        }

        // Record processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Update average processing time
        auto& avg_time = stats_.average_processing_time_us[type];
        auto& count = stats_.dispatch_count_by_type[type];
        
        if (count == 1)
        {
            avg_time = duration.count();
        }
        else
        {
            avg_time = (avg_time * (count - 1) + duration.count()) / count;
        }
    }

    auto MessageDispatcher::set_thread_pool(std::shared_ptr<Thread::ThreadPool> thread_pool) -> void
    {
        thread_pool_ = thread_pool;
    }

    auto MessageDispatcher::start() -> void
    {
        if (is_running_)
        {
            return;
        }
        
        is_running_ = true;
        
        // Start worker threads
        size_t worker_count = 4; // Configurable
        for (size_t i = 0; i < worker_count; ++i)
        {
            workers_.emplace_back(&MessageDispatcher::worker_thread, this);
        }
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "MessageDispatcher started with " + std::to_string(worker_count) + " workers");
    }

    auto MessageDispatcher::stop() -> void
    {
        if (!is_running_)
        {
            return;
        }
        
        is_running_ = false;
        queue_cv_.notify_all();
        
        // Wait for workers to finish
        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        
        workers_.clear();
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "MessageDispatcher stopped");
    }

    auto MessageDispatcher::queue_message(std::shared_ptr<GameSession> session, 
                                         std::unique_ptr<GamePacket> packet) -> void
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            
            QueuedMessage msg;
            msg.session = session;
            msg.packet = std::move(packet);
            msg.priority = msg.packet->get_priority();
            msg.queued_time = std::chrono::steady_clock::now();
            
            message_queue_.push(std::move(msg));
        }
        
        queue_cv_.notify_one();
    }

    auto MessageDispatcher::worker_thread() -> void
    {
        while (is_running_)
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            queue_cv_.wait(lock, [this] 
            { 
                return !message_queue_.empty() || !is_running_; 
            });
            
            if (!is_running_)
            {
                break;
            }
            
            if (!message_queue_.empty())
            {
                auto& top_msg = const_cast<QueuedMessage&>(message_queue_.top());
                auto msg = std::move(top_msg);
                message_queue_.pop();
                lock.unlock();
                
                // Process message
                dispatch(msg.session, *msg.packet);
            }
        }
    }

    auto MessageDispatcher::check_rate_limit(const std::string& session_id, PacketType type) -> bool
    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto key = session_id + "_" + std::to_string(static_cast<uint16_t>(type));
        
        // Get rate limit for this packet type
        auto limit_it = rate_limits_.find(type);
        if (limit_it == rate_limits_.end())
        {
            return true; // No limit set
        }
        
        uint32_t max_per_second = limit_it->second;

        // Get or create rate limit info
        auto& info = rate_limit_info_[key];
        
        // Remove old entries (older than 1 second)
        auto one_second_ago = now - std::chrono::seconds(1);
        while (!info.timestamps.empty() && info.timestamps.front() < one_second_ago)
        {
            info.timestamps.pop();
        }
        
        // Check if limit exceeded
        if (info.timestamps.size() >= max_per_second)
        {
            return false;
        }
        
        // Add current timestamp
        info.timestamps.push(now);
        return true;
    }

    auto MessageDispatcher::set_rate_limit(PacketType type, uint32_t max_per_second) -> void
    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);
        rate_limits_[type] = max_per_second;
    }

    auto MessageDispatcher::enable_rate_limiting(bool enable) -> void
    {
        rate_limiting_enabled_ = enable;
    }

    auto MessageDispatcher::is_rate_limiting_enabled() const -> bool
    {
        return rate_limiting_enabled_;
    }

    auto MessageDispatcher::get_stats() const -> DispatcherStats
    {
        return stats_;
    }

    auto MessageDispatcher::reset_stats() -> void
    {
        stats_.total_dispatched = 0;
        stats_.failed_dispatches = 0;
        stats_.rate_limited = 0;
        stats_.dispatch_count_by_type.clear();
        stats_.average_processing_time_us.clear();
    }

    auto MessageDispatcher::register_default_handlers() -> void
    {
        // Register heartbeat handler
        register_handler(PacketType::Heartbeat, 
            [](std::shared_ptr<GameSession> session, const GamePacket& packet)
            {
                // Update session activity
                session->update_last_activity();
                return std::make_tuple(true, std::nullopt);
            });
        
        // Register disconnect handler
        register_handler(PacketType::Disconnect,
            [](std::shared_ptr<GameSession> session, const GamePacket& packet)
            {
                // Handle graceful disconnect
                session->unbind_connection();
                return std::make_tuple(true, std::nullopt);
            });
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Default handlers registered");
    }
}
