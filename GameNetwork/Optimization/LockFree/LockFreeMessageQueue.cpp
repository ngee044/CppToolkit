#include "LockFreeMessageQueue.h"
#include <thread>
#include <chrono>

namespace GameNetwork::Optimization
{
    namespace // anonymous namespace for internal helpers
    {
        size_t round_up_power_of_2(size_t n)
        {
            if (n <= 1) return 1;
            n--;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            if (sizeof(size_t) > 4)
            {
                n |= n >> 32;
            }
            n++;
            return n;
        }
    }
    
    LockFreeMessageQueue::LockFreeMessageQueue(size_t queue_size)
        : capacity_(round_up_power_of_2(queue_size))
        , buffer_(std::make_unique<Cell[]>(capacity_))
    {
        // Initialize sequence numbers
        for (size_t i = 0; i < capacity_; ++i)
        {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    LockFreeMessageQueue::~LockFreeMessageQueue()
    {
        // Clean up any remaining packets
        while (auto packet = dequeue())
        {
            // Packets will be automatically deleted
        }
    }
    
    LockFreeMessageQueue::LockFreeMessageQueue(LockFreeMessageQueue&& other) noexcept
        : capacity_(other.capacity_)
        , buffer_(std::move(other.buffer_))
        , enqueue_pos_(other.enqueue_pos_.load())
        , dequeue_pos_(other.dequeue_pos_.load())
    {
    }
    
    LockFreeMessageQueue& LockFreeMessageQueue::operator=(LockFreeMessageQueue&& other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing packets
            while (auto packet = dequeue())
            {
                // Packets will be automatically deleted
            }
            
            buffer_ = std::move(other.buffer_);
            enqueue_pos_.store(other.enqueue_pos_.load());
            dequeue_pos_.store(other.dequeue_pos_.load());
        }
        return *this;
    }
    
    auto LockFreeMessageQueue::enqueue(std::unique_ptr<GamePacket> packet) -> bool
    {
        if (!packet)
        {
            return false;
        }
        
        Cell* cell = nullptr;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        
        for (;;)
        {
            cell = &buffer_[wrap_position(pos)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            
            if (dif == 0)
            {
                // Cell is ready for enqueue
                if (enqueue_pos_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (dif < 0)
            {
                // Queue is full
                return false;
            }
            else
            {
                // Another thread is ahead, retry
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
        
        // We have exclusive access to this cell
        cell->packet = std::move(packet);
        cell->sequence.store(pos + 1, std::memory_order_release);
        
        return true;
    }
    
    auto LockFreeMessageQueue::dequeue() -> std::unique_ptr<GamePacket>
    {
        Cell* cell = nullptr;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        
        for (;;)
        {
            cell = &buffer_[wrap_position(pos)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = static_cast<intptr_t>(seq) - 
                           static_cast<intptr_t>(pos + 1);
            
            if (dif == 0)
            {
                // Cell has data ready
                if (dequeue_pos_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (dif < 0)
            {
                // Queue is empty
                return nullptr;
            }
            else
            {
                // Another thread is ahead, retry
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        
        // We have exclusive access to this cell
        auto packet = std::move(cell->packet);
        cell->sequence.store(pos + capacity_, std::memory_order_release);
        
        return packet;
    }
    
    auto LockFreeMessageQueue::dequeue_wait(std::chrono::milliseconds timeout) 
        -> std::unique_ptr<GamePacket>
    {
        auto start = std::chrono::steady_clock::now();
        auto packet = dequeue();
        
        while (!packet && 
               std::chrono::steady_clock::now() - start < timeout)
        {
            std::this_thread::yield();
            packet = dequeue();
        }
        
        return packet;
    }
    
    auto LockFreeMessageQueue::size() const -> size_t
    {
        size_t enq = enqueue_pos_.load(std::memory_order_relaxed);
        size_t deq = dequeue_pos_.load(std::memory_order_relaxed);
        return enq - deq;
    }
    
    auto LockFreeMessageQueue::is_empty() const -> bool
    {
        return size() == 0;
    }
    
    auto LockFreeMessageQueue::is_full() const -> bool
    {
        return size() >= capacity_;
    }
    
} // namespace GameNetwork::Optimization
