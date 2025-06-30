#include "DBCache.h"

#include <Logger.h>
#include <Converter.h>
#include <RedisClient.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <regex>
#include <algorithm>

using namespace Utilities;

namespace GameDatabase
{
    MemoryCacheBackend::MemoryCacheBackend(std::size_t max_size_bytes, CacheEvictionPolicy policy)
        : max_size_bytes_(max_size_bytes)
        , current_size_bytes_(0)
        , eviction_policy_(policy)
    {
        statistics_.total_hits = 0;
        statistics_.total_misses = 0;
        statistics_.total_evictions = 0;
        statistics_.current_size_bytes = 0;
        statistics_.current_entry_count = 0;
        statistics_.hit_rate = 0.0;
    }

    auto MemoryCacheBackend::get(const std::string& key) 
        -> std::tuple<bool, std::optional<std::string>, std::any>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            statistics_.total_misses++;
            return { false, "Key not found", std::any{} };
        }
        
        auto& entry = it->second;
        
        auto now = std::chrono::steady_clock::now();
        if (entry.expire_time != std::chrono::steady_clock::time_point::max() && 
            now > entry.expire_time)
        {
            current_size_bytes_ -= entry.size_bytes;
            cache_.erase(it);
            statistics_.total_misses++;
            statistics_.current_entry_count--;
            return { false, "Key expired", std::any{} };
        }
        
        entry.last_access_time = now;
        entry.access_count++;
        
        statistics_.total_hits++;
        statistics_.hit_rate = static_cast<double>(statistics_.total_hits) / (statistics_.total_hits + statistics_.total_misses);
        
        return { true, std::nullopt, entry.data };
    }

    auto MemoryCacheBackend::set(const std::string& key, const std::any& value, std::chrono::seconds ttl) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        std::size_t value_size = calculate_size(value);
        
        // Check if we need to evict
        if (current_size_bytes_ + value_size > max_size_bytes_)
        {
            evict_if_needed(value_size);
        }
        
        // Still not enough space?
        if (current_size_bytes_ + value_size > max_size_bytes_)
        {
            return { false, "Value too large for cache" };
        }
        
        // Remove old entry if exists
        auto it = cache_.find(key);
        if (it != cache_.end())
        {
            current_size_bytes_ -= it->second.size_bytes;
        }
        else
        {
            statistics_.current_entry_count++;
        }
        
        // Create new entry
        CacheEntry entry;
        entry.data = value;
        entry.size_bytes = value_size;
        entry.access_count = 0;
        
        auto now = std::chrono::steady_clock::now();
        entry.last_access_time = now;
        
        if (ttl.count() > 0)
        {
            entry.expire_time = now + ttl;
        }
        else
        {
            entry.expire_time = std::chrono::steady_clock::time_point::max();
        }
        
        cache_[key] = std::move(entry);
        current_size_bytes_ += value_size;
        statistics_.current_size_bytes = current_size_bytes_;
        
        return { true, std::nullopt };
    }

    auto MemoryCacheBackend::remove(const std::string& key) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            return { false, "Key not found" };
        }
        
        current_size_bytes_ -= it->second.size_bytes;
        statistics_.current_size_bytes = current_size_bytes_;
        statistics_.current_entry_count--;
        
        cache_.erase(it);
        return { true, std::nullopt };
    }

    auto MemoryCacheBackend::exists(const std::string& key) -> bool
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            return false;
        }
        
        // Check if expired
        auto now = std::chrono::steady_clock::now();
        if (it->second.expire_time != std::chrono::steady_clock::time_point::max() && 
            now > it->second.expire_time)
        {
            // Remove expired entry
            current_size_bytes_ -= it->second.size_bytes;
            statistics_.current_entry_count--;
            cache_.erase(it);
            return false;
        }
        
        return true;
    }

    auto MemoryCacheBackend::clear() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        cache_.clear();
        current_size_bytes_ = 0;
        statistics_.current_size_bytes = 0;
        statistics_.current_entry_count = 0;
        
        return { true, std::nullopt };
    }

    auto MemoryCacheBackend::get_statistics() -> CacheStatistics
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return statistics_;
    }

    auto MemoryCacheBackend::evict_if_needed(std::size_t required_size) -> void
    {
        evict_expired();
        
        while (current_size_bytes_ + required_size > max_size_bytes_ && !cache_.empty())
        {
            switch (eviction_policy_)
            {
                case CacheEvictionPolicy::LRU:
                    evict_lru();
                    break;
                case CacheEvictionPolicy::LFU:
                    evict_lfu();
                    break;
                case CacheEvictionPolicy::FIFO:
                    evict_fifo();
                    break;
                case CacheEvictionPolicy::TTL:
                    evict_expired();
                    if (current_size_bytes_ + required_size > max_size_bytes_)
                    {
                        evict_lru(); // Fallback to LRU
                    }
                    break;
            }
            
            statistics_.total_evictions++;
        }
    }

    auto MemoryCacheBackend::evict_lru() -> void
    {
        if (cache_.empty())
        {
            return;
        }
        
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
        {
            if (it->second.last_access_time < oldest->second.last_access_time)
            {
                oldest = it;
            }
        }
        
        current_size_bytes_ -= oldest->second.size_bytes;
        statistics_.current_size_bytes = current_size_bytes_;
        statistics_.current_entry_count--;
        cache_.erase(oldest);
    }

    auto MemoryCacheBackend::evict_lfu() -> void
    {
        if (cache_.empty())
        {
            return;
        }
        
        auto least_used = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
        {
            if (it->second.access_count < least_used->second.access_count)
            {
                least_used = it;
            }
        }
        
        current_size_bytes_ -= least_used->second.size_bytes;
        statistics_.current_size_bytes = current_size_bytes_;
        statistics_.current_entry_count--;
        cache_.erase(least_used);
    }

    auto MemoryCacheBackend::evict_fifo() -> void {
        evict_lru();
    }

    auto MemoryCacheBackend::evict_expired() -> void
    {
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> to_remove;
        
        for (const auto& [key, entry] : cache_)
        {
            if (entry.expire_time != std::chrono::steady_clock::time_point::max() && 
                now > entry.expire_time)
            {
                to_remove.push_back(key);
            }
        }
        
        for (const auto& key : to_remove)
        {
            auto it = cache_.find(key);
            if (it != cache_.end())
            {
                current_size_bytes_ -= it->second.size_bytes;
                statistics_.current_entry_count--;
                cache_.erase(it);
            }
        }
        
        statistics_.current_size_bytes = current_size_bytes_;
    }

    auto MemoryCacheBackend::calculate_size(const std::any& value) -> std::size_t
    {
        if (value.type() == typeid(std::string))
        {
            return std::any_cast<std::string>(value).size();
        }
        else if (value.type() == typeid(int))
        {
            return sizeof(int);
        }
        else if (value.type() == typeid(long))
        {
            return sizeof(long);
        }
        else if (value.type() == typeid(double))
        {
            return sizeof(double);
        }
        else if (value.type() == typeid(std::vector<uint8_t>))
        {
            return std::any_cast<std::vector<uint8_t>>(value).size();
        }
        
        // Default size for unknown types
        return 64;
    }

    // DBCache implementation
    DBCache::DBCache(std::unique_ptr<ICacheBackend> backend)
        : backend_(std::move(backend))
    {
        if (!backend_)
        {
            backend_ = std::make_unique<MemoryCacheBackend>();
        }
    }

    DBCache::~DBCache() = default;

    auto DBCache::remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        return backend_->remove(key);
    }

    auto DBCache::remove_by_pattern(const std::string& pattern) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        
        // Convert pattern to regex
        std::string regex_pattern = pattern;
        
        // Simple pattern matching: * -> .*, ? -> .
        size_t pos = 0;
        while ((pos = regex_pattern.find('*', pos)) != std::string::npos)
        {
            regex_pattern.replace(pos, 1, ".*");
            pos += 2;
        }
        
        pos = 0;
        while ((pos = regex_pattern.find('?', pos)) != std::string::npos)
        {
            regex_pattern.replace(pos, 1, ".");
            pos += 1;
        }
        
        try
        {
            std::regex regex(regex_pattern);
            
            Logger::handle().write(LogTypes::Information, "Pattern-based cache removal not fully implemented");
            
            return { true, std::nullopt };
        }
        catch (const std::regex_error& e)
        {
            return { false, fmt::format("Invalid pattern: {}", e.what()) };
        }
    }

    auto DBCache::exists(const std::string& key) -> bool
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        return backend_->exists(key);
    }

    auto DBCache::clear() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        return backend_->clear();
    }

    auto DBCache::set_backend(std::unique_ptr<ICacheBackend> backend) -> void
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        if (backend)
        {
            backend_ = std::move(backend);
        }
    }

    auto DBCache::get_statistics() -> CacheStatistics
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        return backend_->get_statistics();
    }

    RedisCacheBackend::RedisCacheBackend(const std::string& host, std::uint16_t port, const std::optional<std::string>& password, std::int32_t db_index)
        : host_(host), port_(port), password_(password), db_index_(db_index)
    {
        redis_client_ = std::make_shared<Redis::RedisClient>(host, static_cast<int>(port), Redis::TLSOptions(), db_index);
        
        auto [connected, error] = redis_client_->connect();
        if (!connected)
        {
            Logger::handle().write(LogTypes::Error, fmt::format("Failed to connect to Redis: {}", error.value_or("Unknown error")));
        }
    }

    auto RedisCacheBackend::get(const std::string& key) 
        -> std::tuple<bool, std::optional<std::string>, std::any>
    {
        try
        {
            if (!redis_client_ || !redis_client_->is_connected())
            {
                return {false, "Redis client not connected", std::any{}};
            }
            
            auto [value, error] = redis_client_->get(key);
            if (error)
            {
                return {false, *error, std::any{}};
            }
            
            if (value.empty())
            {
                return {false, "Key not found", std::any{}};
            }
            
            return {true, std::nullopt, std::any(value)};
        }
        catch (const std::exception& e)
        {
            return {false, fmt::format("Redis get error: {}", e.what()), std::any{}};
        }
    }

    auto RedisCacheBackend::set(const std::string& key, const std::any& value, std::chrono::seconds ttl) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!redis_client_ || !redis_client_->is_connected())
            {
                return {false, "Redis client not connected"};
            }
            
            std::string str_value;
            if (value.type() == typeid(std::string))
            {
                str_value = std::any_cast<std::string>(value);
            }
            else
            {
                return {false, "Unsupported value type for Redis storage"};
            }
            
            if (ttl.count() > 0)
            {
                auto [success, error] = redis_client_->set(key, str_value, static_cast<std::uint32_t>(ttl.count()));
                return {success, error};
            }
            else
            {
                auto [success, error] = redis_client_->set(key, str_value);
                return {success, error};
            }
        }
        catch (const std::exception& e)
        {
            return {false, fmt::format("Redis set error: {}", e.what())};
        }
    }

    auto RedisCacheBackend::remove(const std::string& key) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!redis_client_ || !redis_client_->is_connected())
            {
                return {false, "Redis client not connected"};
            }
            
            auto [success, error] = redis_client_->del(key);
            return {success, error};
        }
        catch (const std::exception& e)
        {
            return {false, fmt::format("Redis remove error: {}", e.what())};
        }
    }

    auto RedisCacheBackend::exists(const std::string& key) -> bool
    {
        try
        {
            if (!redis_client_ || !redis_client_->is_connected())
            {
                return false;
            }
            
            auto [exists_result, error] = redis_client_->exists(key);
            return exists_result;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    auto RedisCacheBackend::clear() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!redis_client_ || !redis_client_->is_connected())
            {
                return {false, "Redis client not connected"};
            }
            
            // flush_db clears the current database
            auto [success, error] = redis_client_->flush_db();
            return {success, error};
        }
        catch (const std::exception& e)
        {
            return {false, fmt::format("Redis clear error: {}", e.what())};
        }
    }

    auto RedisCacheBackend::get_statistics() -> CacheStatistics
    {
        CacheStatistics stats{};
        
        try
        {
            if (!redis_client_ || !redis_client_->is_connected())
            {
                return stats;
            }
            
            // Initialize basic statistics
            stats.total_requests = 0;
            stats.total_hits = 0;  
            stats.total_misses = 0;
            stats.hit_rate = 0.0;
            stats.current_size_bytes = 0;
            stats.total_evictions = 0; 

            auto [dbsize, error] = redis_client_->get_db_size();
            if (!error)
            {
                stats.current_entry_count = static_cast<std::size_t>(dbsize);
            }
        }
        catch (const std::exception&)
        {
            // Return empty stats on error
        }
        
        return stats;
    }
}
