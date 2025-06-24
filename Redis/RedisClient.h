#pragma once

#include "RedisConnector.h"

#include <string>
#include <tuple>
#include <optional>
#include <vector>
#include <utility>
#include <unordered_map>
#include <chrono>

namespace Redis
{
	class RedisClient
	{
	public:
		RedisClient(const std::string& host, const int& port, const TLSOptions& tls_options = TLSOptions(), const int& db_index = 0);
		~RedisClient();

		// Connection management
		auto connect() -> std::tuple<bool, std::optional<std::string>>;
		auto is_connected() const -> bool;
		auto disconnect() -> std::tuple<bool, std::optional<std::string>>;
		auto ping() -> std::tuple<bool, std::optional<std::string>>;
		auto select_db(int db_index) -> std::tuple<bool, std::optional<std::string>>;

		// Basic operations
		auto set(const std::string& key, const std::string& value, std::uint32_t ttl_sec = 0) -> std::tuple<bool, std::optional<std::string>>;
		auto get(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>;
		auto del(const std::string& key) -> std::tuple<bool, std::optional<std::string>>;
		auto exists(const std::string& key) -> std::tuple<bool, std::optional<std::string>>;
		auto set_ttl(const std::string& key, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>;
		auto get_ttl(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>;

		// Batch operations
		auto mset(const std::unordered_map<std::string, std::string>& key_values) -> std::tuple<bool, std::optional<std::string>>;
		auto mget(const std::vector<std::string>& keys) -> std::tuple<std::vector<std::string>, std::optional<std::string>>;
		auto del_multiple(const std::vector<std::string>& keys) -> std::tuple<int64_t, std::optional<std::string>>;

		// Atomic operations
		auto incr(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>;
		auto decr(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>;
		auto incrby(const std::string& key, int64_t increment) -> std::tuple<int64_t, std::optional<std::string>>;
		auto decrby(const std::string& key, int64_t decrement) -> std::tuple<int64_t, std::optional<std::string>>;

		// List operations
		auto lpush(const std::string& key, const std::string& value) -> std::tuple<int64_t, std::optional<std::string>>;
		auto rpush(const std::string& key, const std::string& value) -> std::tuple<int64_t, std::optional<std::string>>;
		auto lpop(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>;
		auto rpop(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>;
		auto llen(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>;
		auto lrange(const std::string& key, int64_t start, int64_t stop) -> std::tuple<std::vector<std::string>, std::optional<std::string>>;

		// Hash operations
		auto hset(const std::string& key, const std::string& field, const std::string& value) -> std::tuple<bool, std::optional<std::string>>;
		auto hget(const std::string& key, const std::string& field) -> std::tuple<std::string, std::optional<std::string>>;
		auto hmset(const std::string& key, const std::unordered_map<std::string, std::string>& field_values) -> std::tuple<bool, std::optional<std::string>>;
		auto hmget(const std::string& key, const std::vector<std::string>& fields) -> std::tuple<std::vector<std::string>, std::optional<std::string>>;
		auto hgetall(const std::string& key) -> std::tuple<std::unordered_map<std::string, std::string>, std::optional<std::string>>;
		auto hdel(const std::string& key, const std::string& field) -> std::tuple<bool, std::optional<std::string>>;
		auto hexists(const std::string& key, const std::string& field) -> std::tuple<bool, std::optional<std::string>>;

		// Set operations
		auto sadd(const std::string& key, const std::string& member) -> std::tuple<bool, std::optional<std::string>>;
		auto srem(const std::string& key, const std::string& member) -> std::tuple<bool, std::optional<std::string>>;
		auto smembers(const std::string& key) -> std::tuple<std::vector<std::string>, std::optional<std::string>>;
		auto sismember(const std::string& key, const std::string& member) -> std::tuple<bool, std::optional<std::string>>;
		auto scard(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>;

		// Sorted set operations
		auto zadd(const std::string& key, double score, const std::string& member) -> std::tuple<bool, std::optional<std::string>>;
		auto zrem(const std::string& key, const std::string& member) -> std::tuple<bool, std::optional<std::string>>;
		auto zrange(const std::string& key, int64_t start, int64_t stop, bool with_scores = false) -> std::tuple<std::vector<std::pair<std::string, double>>, std::optional<std::string>>;
		auto zrevrange(const std::string& key, int64_t start, int64_t stop, bool with_scores = false) -> std::tuple<std::vector<std::pair<std::string, double>>, std::optional<std::string>>;
		auto zscore(const std::string& key, const std::string& member) -> std::tuple<double, std::optional<std::string>>;
		auto zcard(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>;

		// Transaction operations
		auto multi() -> std::tuple<bool, std::optional<std::string>>;
		auto exec() -> std::tuple<std::vector<std::string>, std::optional<std::string>>;
		auto discard() -> std::tuple<bool, std::optional<std::string>>;

		// Pub/Sub operations
		auto publish(const std::string& channel, const std::string& message) -> std::tuple<int64_t, std::optional<std::string>>;
		auto subscribe(const std::string& channel) -> std::tuple<bool, std::optional<std::string>>;
		auto unsubscribe(const std::string& channel) -> std::tuple<bool, std::optional<std::string>>;

		// Utility operations
		auto flush_db() -> std::tuple<bool, std::optional<std::string>>;
		auto flush_all() -> std::tuple<bool, std::optional<std::string>>;
		auto get_db_size() -> std::tuple<int64_t, std::optional<std::string>>;

	private:
		std::shared_ptr<RedisConnector> redis_connector_;
	};
}
