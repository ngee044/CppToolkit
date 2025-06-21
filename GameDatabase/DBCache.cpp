#include "DBCache.h"
#include <algorithm>
#include <regex>

namespace GameDataBase
{
    // MemoryCacheBackend 구현
    MemoryCacheBackend::MemoryCacheBackend(std::size_t max_size_bytes, CacheEvictionPolicy policy)
        : max_size_bytes_(max_size_bytes)
        , current_size_bytes_(0)
        , eviction_policy_(policy)
        , statistics_{}
    {
    }

    auto MemoryCacheBackend::get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::any>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            statistics_.total_misses++;
            return { false, "Key not found", std::any{} };
        }
        
        auto& entry = it->second;
        
        // TTL 확인
        if (entry.expire_time < std::chrono::steady_clock::now())
        {
            current_size_bytes_ -= entry.size_bytes;
            cache_.erase(it);
            statistics_.total_misses++;
            statistics_.total_evictions++;
            return { false, "Key expired", std::any{} };
        }
        
        // 접근 정보 업데이트
        entry.last_access_time = std::chrono::steady_clock::now();
        entry.access_count++;
        
        statistics_.total_hits++;
        statistics_.hit_rate = static_cast<double>(statistics_.total_hits) / 
                              (statistics_.total_hits + statistics_.total_misses);
        
        return { true, std::nullopt, entry.data };
    }

    auto MemoryCacheBackend::set(const std::string& key, const std::any& value, std::chrono::seconds ttl) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        std::size_t value_size = calculate_size(value);
        
        // 기존 항목이 있으면 크기 업데이트
        auto existing_it = cache_.find(key);
        if (existing_it != cache_.end())
        {
            current_size_bytes_ -= existing_it->second.size_bytes;
            cache_.erase(existing_it);
        }
        
        // 필요시 eviction 수행
        evict_if_needed(value_size);
        
        // 새 항목 추가
        CacheEntry entry;
        entry.data = value;
        entry.expire_time = std::chrono::steady_clock::now() + ttl;
        entry.last_access_time = std::chrono::steady_clock::now();
        entry.access_count = 0;
        entry.size_bytes = value_size;
        
        cache_[key] = std::move(entry);
        current_size_bytes_ += value_size;
        
        statistics_.current_size_bytes = current_size_bytes_;
        statistics_.current_entry_count = cache_.size();
        
        return { true, std::nullopt };
    }

    auto MemoryCacheBackend::remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            return { false, "Key not found" };
        }
        
        current_size_bytes_ -= it->second.size_bytes;
        cache_.erase(it);
        
        statistics_.current_size_bytes = current_size_bytes_;
        statistics_.current_entry_count = cache_.size();
        
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
        
        // TTL 확인
        if (it->second.expire_time < std::chrono::steady_clock::now())
        {
            current_size_bytes_ -= it->second.size_bytes;
            cache_.erase(it);
            statistics_.total_evictions++;
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
        // 만료된 항목 먼저 제거
        evict_expired();
        
        // 여전히 공간이 부족하면 정책에 따라 제거
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
                        evict_lru(); // TTL 정책에서도 공간 부족시 LRU 사용
                    }
                    break;
            }
        }
    }

    auto MemoryCacheBackend::evict_lru() -> void
    {
        if (cache_.empty()) return;
        
        auto oldest = std::min_element(cache_.begin(), cache_.end(),
            [](const auto& a, const auto& b) {
                return a.second.last_access_time < b.second.last_access_time;
            });
            
        current_size_bytes_ -= oldest->second.size_bytes;
        cache_.erase(oldest);
        statistics_.total_evictions++;
    }

    auto MemoryCacheBackend::evict_lfu() -> void
    {
        if (cache_.empty()) return;
        
        auto least_used = std::min_element(cache_.begin(), cache_.end(),
            [](const auto& a, const auto& b) {
                return a.second.access_count < b.second.access_count;
            });
            
        current_size_bytes_ -= least_used->second.size_bytes;
        cache_.erase(least_used);
        statistics_.total_evictions++;
    }

    auto MemoryCacheBackend::evict_fifo() -> void
    {
        // FIFO는 추가 시간 기준으로 구현
        // 실제로는 insertion order를 추적해야 하지만, 여기서는 간단히 구현
        if (!cache_.empty())
        {
            auto it = cache_.begin();
            current_size_bytes_ -= it->second.size_bytes;
            cache_.erase(it);
            statistics_.total_evictions++;
        }
    }

    auto MemoryCacheBackend::evict_expired() -> void
    {
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = cache_.begin(); it != cache_.end(); )
        {
            if (it->second.expire_time < now)
            {
                current_size_bytes_ -= it->second.size_bytes;
                it = cache_.erase(it);
                statistics_.total_evictions++;
            }
            else
            {
                ++it;
            }
        }
    }

    auto MemoryCacheBackend::calculate_size(const std::any& value) -> std::size_t
    {
        // 간단한 크기 추정 (실제로는 더 정교한 계산 필요)
        if (value.type() == typeid(std::string))
        {
            return std::any_cast<std::string>(value).size();
        }
        else if (value.type() == typeid(std::wstring))
        {
            return std::any_cast<std::wstring>(value).size() * sizeof(wchar_t);
        }
        else if (value.type() == typeid(std::vector<std::uint8_t>))
        {
            return std::any_cast<std::vector<std::uint8_t>>(value).size();
        }
        else
        {
            // 기본 타입들의 추정 크기
            return 64; // 기본값
        }
    }

    // RedisCacheBackend 구현 (스텁)
    RedisCacheBackend::RedisCacheBackend(const std::string& host, std::uint16_t port,
                                       const std::optional<std::string>& password,
                                       std::int32_t db_index)
        : host_(host)
        , port_(port)
        , password_(password)
        , db_index_(db_index)
    {
        // Redis 연결 초기화는 실제 Redis 모듈에서 구현
    }

    auto RedisCacheBackend::get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::any>
    {
        // Redis 모듈에서 실제 구현
        return { false, "Redis backend not implemented", std::any{} };
    }

    auto RedisCacheBackend::set(const std::string& key, const std::any& value, std::chrono::seconds ttl) -> std::tuple<bool, std::optional<std::string>>
    {
        // Redis 모듈에서 실제 구현
        return { false, "Redis backend not implemented" };
    }

    auto RedisCacheBackend::remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>>
    {
        // Redis 모듈에서 실제 구현
        return { false, "Redis backend not implemented" };
    }

    auto RedisCacheBackend::exists(const std::string& key) -> bool
    {
        // Redis 모듈에서 실제 구현
        return false;
    }

    auto RedisCacheBackend::clear() -> std::tuple<bool, std::optional<std::string>>
    {
        // Redis 모듈에서 실제 구현
        return { false, "Redis backend not implemented" };
    }

    auto RedisCacheBackend::get_statistics() -> CacheStatistics
    {
        // Redis 모듈에서 실제 구현
        return CacheStatistics{};
    }

    // DBCache 구현
    DBCache::DBCache(std::unique_ptr<ICacheBackend> backend)
        : backend_(std::move(backend))
    {
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
        
        // 패턴 매칭 (간단한 와일드카드 지원)
        std::regex pattern_regex(pattern);
        std::int32_t removed_count = 0;
        
        // 메모리 백엔드의 경우 직접 순회
        if (auto* memory_backend = dynamic_cast<MemoryCacheBackend*>(backend_.get()))
        {
            // 패턴 매칭하여 제거 (실제 구현은 백엔드 내부에서 수행해야 함)
            // 여기서는 인터페이스만 제공
            return { true, std::nullopt };
        }
        
        return { false, "Pattern-based removal not supported by current backend" };
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
        backend_ = std::move(backend);
    }

    auto DBCache::get_statistics() -> CacheStatistics
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        return backend_->get_statistics();
    }
}
