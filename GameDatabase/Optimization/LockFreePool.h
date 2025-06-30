#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <vector>
#include <thread>

namespace GameDatabase::Optimization
{
	/**
	 * Lock-free pool for managing database connections
	 * Uses Treiber stack algorithm for high concurrency
	 */
	template<typename T>
	class LockFreePool
	{
	private:
		struct Node
		{
			T data;
			std::atomic<Node*> next;
            
			explicit Node(T&& value) 
				: data(std::move(value)), next(nullptr) {}
		};
        
	public:
		LockFreePool() : head_(nullptr), size_(0) {}
        
		~LockFreePool()
		{
			// Clean up all nodes
			Node* current = head_.load(std::memory_order_relaxed);
			while (current)
			{
				Node* next = current->next.load(std::memory_order_relaxed);
				delete current;
				current = next;
			}
		}
        
		// Disable copy
		LockFreePool(const LockFreePool&) = delete;
		LockFreePool& operator=(const LockFreePool&) = delete;
        
		/**
		 * Push an item into the pool
		 * Thread-safe and wait-free
		 */
		auto push(T item) -> void
		{
			Node* new_node = new Node(std::move(item));
			Node* old_head = head_.load(std::memory_order_relaxed);
            
			do
			{
				new_node->next.store(old_head, std::memory_order_relaxed);
			} while (!head_.compare_exchange_weak(
				old_head, new_node,
				std::memory_order_release,
				std::memory_order_relaxed));
                
			size_.fetch_add(1, std::memory_order_relaxed);
		}
        
		/**
		 * Pop an item from the pool
		 * Returns nullopt if pool is empty
		 */
		auto pop() -> std::optional<T>
		{
			Node* old_head = head_.load(std::memory_order_acquire);
            
			while (old_head)
			{
				Node* next = old_head->next.load(std::memory_order_relaxed);
                
				if (head_.compare_exchange_weak(
					old_head, next,
					std::memory_order_release,
					std::memory_order_acquire))
				{
					T data = std::move(old_head->data);
					delete old_head;
					size_.fetch_sub(1, std::memory_order_relaxed);
					return data;
				}
			}
            
			return std::nullopt;
		}
        
		/**
		 * Try to pop with a custom retry count
		 */
		auto try_pop(int max_retries = 3) -> std::optional<T>
		{
			for (int i = 0; i < max_retries; ++i)
			{
				if (auto item = pop())
				{
					return item;
				}
				std::this_thread::yield();
			}
			return std::nullopt;
		}
        
		/**
		 * Get current pool size (approximate)
		 */
		auto size() const -> size_t
		{
			return size_.load(std::memory_order_relaxed);
		}
        
		/**
		 * Check if pool is empty
		 */
		auto is_empty() const -> bool
		{
			return head_.load(std::memory_order_acquire) == nullptr;
		}
        
		/**
		 * Reserve capacity by pre-allocating items
		 */
		template<typename... Args>
		auto reserve(size_t count, Args&&... args) -> void
		{
			for (size_t i = 0; i < count; ++i)
			{
				push(T(std::forward<Args>(args)...));
			}
		}
        
	private:
		alignas(64) std::atomic<Node*> head_;
		alignas(64) std::atomic<size_t> size_;
	};
    
} // namespace GameDatabase::Optimization
