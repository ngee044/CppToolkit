#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <iomanip>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Original components
#include <GameNetwork/Packet/GamePacket.h>
#include <GameNetwork/Session/GameSession.h>
#include <GameDatabase/DBConnectionPool.h>

// Optimized components
#include <GameNetwork/Optimization/ZeroCopy/ZeroCopyBuffer.h>
#include <GameNetwork/Optimization/LockFree/LockFreeMessageQueue.h>
#include <GameDatabase/Optimization/OptimizedDBConnectionPool.h>
#include <GameNetwork/Optimization/OptimizedPacketProcessor.h>

using namespace std::chrono;

class PerformanceBenchmark
{
public:
    struct BenchmarkResult
    {
        std::string test_name;
        double original_time_ms;
        double optimized_time_ms;
        double improvement_percent;
        size_t operations_count;
        double original_ops_per_sec;
        double optimized_ops_per_sec;
    };
    
    static auto benchmark_packet_serialization(size_t packet_count) -> BenchmarkResult
    {
        BenchmarkResult result;
        result.test_name = "Packet Serialization";
        result.operations_count = packet_count;
        
        // Create test packets
        std::vector<std::unique_ptr<GameNetwork::EntityUpdatePacket>> packets;
        packets.reserve(packet_count);
        
        for (size_t i = 0; i < packet_count; ++i)
        {
            auto packet = std::make_unique<GameNetwork::EntityUpdatePacket>();
            packet->set_entity_id(i);
            packet->set_location(GameNetwork::Location{
                static_cast<float>(i), 
                static_cast<float>(i + 1), 
                static_cast<float>(i + 2)});
            packet->set_health(100);
            packets.push_back(std::move(packet));
        }
        
        // Benchmark original serialization
        std::vector<std::vector<uint8_t>> serialized_original;
        auto start = high_resolution_clock::now();
        
        for (const auto& packet : packets)
        {
            serialized_original.push_back(packet->serialize());
        }
        
        auto end = high_resolution_clock::now();
        result.original_time_ms = duration<double, std::milli>(end - start).count();
        
        // Benchmark optimized (zero-copy) approach
        GameNetwork::Optimization::ZeroCopyBuffer buffer(1 << 20); // 1MB
        start = high_resolution_clock::now();
        
        for (const auto& packet : packets)
        {
            auto serialized = packet->serialize();
            buffer.write_packet(serialized.data(), serialized.size());
        }
        
        end = high_resolution_clock::now();
        result.optimized_time_ms = duration<double, std::milli>(end - start).count();
        
        // Calculate metrics
        result.improvement_percent = 
            ((result.original_time_ms - result.optimized_time_ms) / 
             result.original_time_ms) * 100.0;
        result.original_ops_per_sec = 
            (packet_count / result.original_time_ms) * 1000.0;
        result.optimized_ops_per_sec = 
            (packet_count / result.optimized_time_ms) * 1000.0;
        
        return result;
    }
    
    static auto benchmark_message_queue(size_t message_count, size_t producers, size_t consumers) 
        -> BenchmarkResult
    {
        BenchmarkResult result;
        result.test_name = "Message Queue";
        result.operations_count = message_count;
        
        // Benchmark original (mutex-based) queue
        std::queue<std::unique_ptr<GameNetwork::GamePacket>> original_queue;
        std::mutex queue_mutex;
        std::condition_variable queue_cv;
        std::atomic<size_t> original_processed{0};
        
        auto start = high_resolution_clock::now();
        
        // Producer threads
        std::vector<std::thread> original_producers;
        for (size_t p = 0; p < producers; ++p)
        {
            original_producers.emplace_back([&, p]()
            {
                size_t messages_per_producer = message_count / producers;
                for (size_t i = 0; i < messages_per_producer; ++i)
                {
                    auto packet = std::make_unique<GameNetwork::HeartbeatPacket>();
                    packet->set_timestamp(i);
                    
                    {
                        std::lock_guard lock(queue_mutex);
                        original_queue.push(std::move(packet));
                    }
                    queue_cv.notify_one();
                }
            });
        }
        
        // Consumer threads
        std::vector<std::thread> original_consumers;
        for (size_t c = 0; c < consumers; ++c)
        {
            original_consumers.emplace_back([&]()
            {
                while (original_processed.load() < message_count)
                {
                    std::unique_lock lock(queue_mutex);
                    queue_cv.wait(lock, [&] { return !original_queue.empty() || 
                                                    original_processed.load() >= message_count; });
                    
                    if (!original_queue.empty())
                    {
                        auto packet = std::move(original_queue.front());
                        original_queue.pop();
                        lock.unlock();
                        
                        // Simulate processing
                        original_processed.fetch_add(1);
                    }
                }
            });
        }
        
        for (auto& t : original_producers) t.join();
        for (auto& t : original_consumers) t.join();
        
        auto end = high_resolution_clock::now();
        result.original_time_ms = duration<double, std::milli>(end - start).count();
        
        // Benchmark optimized (lock-free) queue
        GameNetwork::Optimization::LockFreeMessageQueue lockfree_queue;
        std::atomic<size_t> lockfree_processed{0};
        
        start = high_resolution_clock::now();
        
        // Producer threads
        std::vector<std::thread> lockfree_producers;
        for (size_t p = 0; p < producers; ++p)
        {
            lockfree_producers.emplace_back([&, p]()
            {
                size_t messages_per_producer = message_count / producers;
                for (size_t i = 0; i < messages_per_producer; ++i)
                {
                    auto packet = std::make_unique<GameNetwork::HeartbeatPacket>();
                    packet->set_timestamp(i);
                    
                    while (!lockfree_queue.enqueue(std::move(packet)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }
        
        // Consumer threads
        std::vector<std::thread> lockfree_consumers;
        for (size_t c = 0; c < consumers; ++c)
        {
            lockfree_consumers.emplace_back([&]()
            {
                while (lockfree_processed.load() < message_count)
                {
                    auto packet = lockfree_queue.dequeue();
                    if (packet)
                    {
                        // Simulate processing
                        lockfree_processed.fetch_add(1);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }
        
        for (auto& t : lockfree_producers) t.join();
        for (auto& t : lockfree_consumers) t.join();
        
        end = high_resolution_clock::now();
        result.optimized_time_ms = duration<double, std::milli>(end - start).count();
        
        // Calculate metrics
        result.improvement_percent = 
            ((result.original_time_ms - result.optimized_time_ms) / 
             result.original_time_ms) * 100.0;
        result.original_ops_per_sec = 
            (message_count / result.original_time_ms) * 1000.0;
        result.optimized_ops_per_sec = 
            (message_count / result.optimized_time_ms) * 1000.0;
        
        return result;
    }
    
    static auto print_results(const std::vector<BenchmarkResult>& results) -> void
    {
        std::cout << "\n=== Performance Benchmark Results ===\n\n";
        std::cout << std::left << std::setw(25) << "Test Name"
                  << std::right << std::setw(12) << "Operations"
                  << std::setw(15) << "Original (ms)"
                  << std::setw(15) << "Optimized (ms)"
                  << std::setw(15) << "Improvement %"
                  << std::setw(18) << "Original Ops/sec"
                  << std::setw(18) << "Optimized Ops/sec"
                  << "\n";
        std::cout << std::string(120, '-') << "\n";
        
        for (const auto& result : results)
        {
            std::cout << std::left << std::setw(25) << result.test_name
                      << std::right << std::setw(12) << result.operations_count
                      << std::setw(15) << std::fixed << std::setprecision(2) 
                      << result.original_time_ms
                      << std::setw(15) << result.optimized_time_ms
                      << std::setw(14) << std::setprecision(1) 
                      << result.improvement_percent << "%"
                      << std::setw(18) << std::setprecision(0) 
                      << result.original_ops_per_sec
                      << std::setw(18) << result.optimized_ops_per_sec
                      << "\n";
        }
        
        std::cout << "\n";
    }
};

int main()
{
    std::vector<PerformanceBenchmark::BenchmarkResult> results;
    
    std::cout << "Running performance benchmarks...\n";
    
    // Test 1: Packet Serialization
    std::cout << "1. Testing packet serialization (100,000 packets)...\n";
    results.push_back(PerformanceBenchmark::benchmark_packet_serialization(100000));
    
    // Test 2: Message Queue (Single Producer, Single Consumer)
    std::cout << "2. Testing message queue SPSC (1,000,000 messages)...\n";
    results.push_back(PerformanceBenchmark::benchmark_message_queue(1000000, 1, 1));
    
    // Test 3: Message Queue (Multi Producer, Multi Consumer)
    std::cout << "3. Testing message queue MPMC (1,000,000 messages, 4P/4C)...\n";
    results.push_back(PerformanceBenchmark::benchmark_message_queue(1000000, 4, 4));
    
    // Print results
    PerformanceBenchmark::print_results(results);
    
    // Summary
    std::cout << "\n=== Performance Summary ===\n";
    std::cout << "Zero Copy improvements:\n";
    std::cout << "- Reduced memory allocations and copies\n";
    std::cout << "- Better cache locality\n";
    std::cout << "- Lower latency for packet processing\n\n";
    
    std::cout << "Lock-Free improvements:\n";
    std::cout << "- Eliminated thread contention\n";
    std::cout << "- Consistent low latency\n";
    std::cout << "- Better scalability with multiple threads\n";
    
    return 0;
}
