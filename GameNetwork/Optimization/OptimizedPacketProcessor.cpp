#include "OptimizedPacketProcessor.h"
#include <iostream>

namespace GameNetwork::Optimization
{
    OptimizedPacketProcessor::OptimizedPacketProcessor(
        std::shared_ptr<Thread::ThreadPool> thread_pool)
        : thread_pool_(thread_pool)
        , queue_(LockFreeMessageQueue::DEFAULT_QUEUE_SIZE)
    {
    }
    
    OptimizedPacketProcessor::~OptimizedPacketProcessor()
    {
        stop();
    }
    
    auto OptimizedPacketProcessor::register_handler(
        PacketType type, PacketHandler handler) -> void
    {
        std::unique_lock lock(handlers_mutex_);
        handlers_[type] = std::move(handler);
    }
    
    auto OptimizedPacketProcessor::register_factory(
        PacketType type, PacketFactory factory) -> void
    {
        std::unique_lock lock(factories_mutex_);
        factories_[type] = std::move(factory);
    }
    
    auto OptimizedPacketProcessor::process_raw_packet(
        const PacketView& packet_view) -> bool
    {
        if (!packet_view.is_valid() || packet_view.size < sizeof(uint16_t))
        {
            return false;
        }
        
        // Read packet type from first 2 bytes
        uint16_t type_value;
        std::memcpy(&type_value, packet_view.data, sizeof(type_value));
        auto packet_type = static_cast<PacketType>(type_value);
        
        // Find factory for this packet type
        PacketFactory factory;
        {
            std::shared_lock lock(factories_mutex_);
            auto it = factories_.find(packet_type);
            if (it == factories_.end())
            {
                // No factory registered for packet type
                // LOG_WARNING("No factory registered for packet type: {}", static_cast<int>(packet_type));
                std::cerr << "No factory registered for packet type: " << static_cast<int>(packet_type) << std::endl;
                dropped_count_.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            factory = it->second;
        }
        
        // Create packet using factory (zero-copy deserialization)
        auto packet = factory(packet_view);
        if (!packet)
        {
            // Failed to deserialize packet
            // LOG_ERROR("Failed to deserialize packet type: {}", static_cast<int>(packet_type));
            std::cerr << "Failed to deserialize packet type: " << static_cast<int>(packet_type) << std::endl;
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Queue for processing
        return process_packet(std::move(packet));
    }
    
    auto OptimizedPacketProcessor::process_packet(
        std::unique_ptr<GamePacket> packet) -> bool
    {
        if (!packet)
        {
            return false;
        }
        
        // Try to enqueue
        if (!queue_.enqueue(std::move(packet)))
        {
            // LOG_WARNING("Packet queue full, dropping packet");
            std::cerr << "Packet queue full, dropping packet" << std::endl;
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        return true;
    }
    
    auto OptimizedPacketProcessor::start(size_t worker_count) -> void
    {
        if (running_.exchange(true))
        {
            return; // Already running
        }
        
        // Start worker threads
        workers_.reserve(worker_count);
        for (size_t i = 0; i < worker_count; ++i)
        {
            workers_.push_back(
                std::async(std::launch::async, 
                          &OptimizedPacketProcessor::worker_thread, this));
        }
    }
    
    auto OptimizedPacketProcessor::stop() -> void
    {
        if (!running_.exchange(false))
        {
            return; // Not running
        }
        
        // Wait for workers to finish
        for (auto& worker : workers_)
        {
            if (worker.valid())
            {
                worker.wait();
            }
        }
        
        workers_.clear();
    }
    
    auto OptimizedPacketProcessor::worker_thread() -> void
    {
        while (running_.load(std::memory_order_relaxed))
        {
            // Try to get packet with timeout
            auto packet = queue_.dequeue_wait(std::chrono::milliseconds(10));
            
            if (!packet)
            {
                continue; // Timeout or empty
            }
            
            // Find handler
            PacketHandler handler;
            {
                std::shared_lock lock(handlers_mutex_);
                auto it = handlers_.find(packet->get_type());
                if (it == handlers_.end())
                {
                    // LOG_WARNING("No handler registered for packet type: {}", static_cast<int>(packet->get_type()));
                    std::cerr << "No handler registered for packet type: " << static_cast<int>(packet->get_type()) << std::endl;
                    continue;
                }
                handler = it->second;
            }
            
            // Process packet in thread pool
            // Note: We need to use raw pointer because unique_ptr can't be copied into std::function
            auto raw_packet = packet.release();
            auto job = std::make_shared<Thread::Job>(
                Thread::JobPriorities::Normal,
                [handler, raw_packet]() mutable 
                    -> std::tuple<bool, std::optional<std::string>>
                {
                    // Take ownership back
                    auto packet_ptr = std::unique_ptr<GamePacket>(raw_packet);
                    try
                    {
                        handler(std::move(packet_ptr));
                        return {true, std::nullopt};
                    }
                    catch (const std::exception& e)
                    {
                        // LOG_ERROR("Exception in packet handler: {}", e.what());
                        std::cerr << "Exception in packet handler: " << e.what() << std::endl;
                        return {false, e.what()};
                    }
                },
                "PacketHandler");
            
            thread_pool_->add_job(job);
            
            processed_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
} // namespace GameNetwork::Optimization
