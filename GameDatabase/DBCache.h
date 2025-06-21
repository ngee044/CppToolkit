#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <tuple>
#include <mutex>
#include <functional>
#include <variant>
#include <vector>
#include <any>

namespace GameDataBase
{
    // 캐시 항목 구조체
    struct CacheEntry
    {
        std::any data;
        std::chrono::steady_clock::time_point expire_time;
        std::chrono::steady_clock::time_point last_access_time;
        std::size_t access_count;
        std::size_t size_bytes;
    };

    // 캐시 정책
    enum class CacheEvictionPolicy
    {
        LRU,    // Least Recently Used
        LFU,    // Least Frequently Used
        FIFO,   // First In First Out
        TTL     // Time To Live based
    };

    // 캐시 통계
    struct CacheStatistics
    {
        std::size_t total_hits;
        std::size_t total_misses;
        std::size_t total_evictions;
        std::size_t current_size_bytes;
        std::size_t current_entry_count;
        double hit_rate;
    };

    // 캐시 백엔드 인터페이스
    class ICacheBackend
    {
    public:
        virtual ~ICacheBackend() = default;
        
        virtual auto get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::any> = 0;
        virtual auto set(const std::string& key, const std::any& value, std::chrono::seconds ttl) -> std::tuple<bool, std::optional<std::string>> = 0;
        virtual auto remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>> = 0;
        virtual auto exists(const std::string& key) -> bool = 0;
        virtual auto clear() -> std::tuple<bool, std::optional<std::string>> = 0;
        virtual auto get_statistics() -> CacheStatistics = 0;
    };

    // 인메모리 캐시 백엔드
    class MemoryCacheBackend : public ICacheBackend
    {
    public:
        MemoryCacheBackend(std::size_t max_size_bytes = 100 * 1024 * 1024, // 100MB
                          CacheEvictionPolicy policy = CacheEvictionPolicy::LRU);
        
        auto get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::any> override;
        auto set(const std::string& key, const std::any& value, std::chrono::seconds ttl) -> std::tuple<bool, std::optional<std::string>> override;
        auto remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>> override;
        auto exists(const std::string& key) -> bool override;
        auto clear() -> std::tuple<bool, std::optional<std::string>> override;
        auto get_statistics() -> CacheStatistics override;
        
    private:
        auto evict_if_needed(std::size_t required_size) -> void;
        auto evict_lru() -> void;
        auto evict_lfu() -> void;
        auto evict_fifo() -> void;
        auto evict_expired() -> void;
        auto calculate_size(const std::any& value) -> std::size_t;
        
    private:
        std::unordered_map<std::string, CacheEntry> cache_;
        std::size_t max_size_bytes_;
        std::size_t current_size_bytes_;
        CacheEvictionPolicy eviction_policy_;
        CacheStatistics statistics_;
        mutable std::mutex cache_mutex_;
    };

    // Redis 캐시 백엔드 (인터페이스만 정의, 실제 구현은 Redis 모듈에서)
    class RedisCacheBackend : public ICacheBackend
    {
    public:
        RedisCacheBackend(const std::string& host, std::uint16_t port,
                         const std::optional<std::string>& password = std::nullopt,
                         std::int32_t db_index = 0);
        
        auto get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::any> override;
        auto set(const std::string& key, const std::any& value, std::chrono::seconds ttl) -> std::tuple<bool, std::optional<std::string>> override;
        auto remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>> override;
        auto exists(const std::string& key) -> bool override;
        auto clear() -> std::tuple<bool, std::optional<std::string>> override;
        auto get_statistics() -> CacheStatistics override;
        
    private:
        // Redis 연결 정보
        std::string host_;
        std::uint16_t port_;
        std::optional<std::string> password_;
        std::int32_t db_index_;
    };

    // 데이터베이스 캐시 관리자
    class DBCache
    {
    public:
        DBCache(std::unique_ptr<ICacheBackend> backend = std::make_unique<MemoryCacheBackend>());
        ~DBCache();

        // 캐시 조회
        template<typename T>
        auto get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::optional<T>>;
        
        // 캐시 저장
        template<typename T>
        auto set(const std::string& key, const T& value, 
                std::chrono::seconds ttl = std::chrono::seconds(300)) -> std::tuple<bool, std::optional<std::string>>;
        
        // 캐시 삭제
        auto remove(const std::string& key) -> std::tuple<bool, std::optional<std::string>>;
        
        // 패턴 기반 삭제
        auto remove_by_pattern(const std::string& pattern) -> std::tuple<bool, std::optional<std::string>>;
        
        // 캐시 존재 확인
        auto exists(const std::string& key) -> bool;
        
        // 전체 캐시 삭제
        auto clear() -> std::tuple<bool, std::optional<std::string>>;
        
        // 캐시 백엔드 변경
        auto set_backend(std::unique_ptr<ICacheBackend> backend) -> void;
        
        // 통계 정보
        auto get_statistics() -> CacheStatistics;
        
        // 쿼리 결과 캐싱을 위한 헬퍼 함수
        template<typename TResult>
        auto cache_query(const std::string& query_key,
                        std::function<std::tuple<bool, std::optional<std::string>, TResult()>> query_func,
                        std::chrono::seconds ttl = std::chrono::seconds(300)) 
                        -> std::tuple<bool, std::optional<std::string>, std::optional<TResult>>;

    private:
        std::unique_ptr<ICacheBackend> backend_;
        mutable std::mutex backend_mutex_;
    };

    // 템플릿 구현
    template<typename T>
    auto DBCache::get(const std::string& key) -> std::tuple<bool, std::optional<std::string>, std::optional<T>>
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        
        auto [success, error, value] = backend_->get(key);
        if (!success)
        {
            return { false, error, std::nullopt };
        }
        
        try
        {
            T result = std::any_cast<T>(value);
            return { true, std::nullopt, result };
        }
        catch (const std::bad_any_cast& e)
        {
            return { false, "Type mismatch in cache", std::nullopt };
        }
    }

    template<typename T>
    auto DBCache::set(const std::string& key, const T& value, std::chrono::seconds ttl) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        return backend_->set(key, std::any(value), ttl);
    }

    template<typename TResult>
    auto DBCache::cache_query(const std::string& query_key,
                             std::function<std::tuple<bool, std::optional<std::string>, TResult()>> query_func,
                             std::chrono::seconds ttl) 
                             -> std::tuple<bool, std::optional<std::string>, std::optional<TResult>>
    {
        // 캐시에서 먼저 조회
        auto [cache_hit, cache_error, cached_value] = get<TResult>(query_key);
        if (cache_hit && cached_value.has_value())
        {
            return { true, std::nullopt, cached_value };
        }
        
        // 캐시 미스인 경우 쿼리 실행
        auto [query_success, query_error, query_result] = query_func();
        if (!query_success)
        {
            return { false, query_error, std::nullopt };
        }
        
        // 결과를 캐시에 저장
        auto [set_success, set_error] = set(query_key, query_result, ttl);
        if (!set_success)
        {
            // 캐시 저장 실패는 경고만 하고 결과는 반환
            // 로그를 남기거나 통계에 기록
        }
        
        return { true, std::nullopt, query_result };
    }
}
