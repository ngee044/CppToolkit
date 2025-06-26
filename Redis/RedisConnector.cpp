#include "RedisConnector.h"
#include <Logger.h>
#include <Converter.h>

using namespace Utilities;

namespace Redis
{
    RedisConnector::RedisConnector(const std::string& host, int port, const TLSOptions& tlsOptions, const int& db_index)
        : host_(host), port_(port), tlsOptions_(tlsOptions), db_index_(db_index), is_connected_(false)
    {
        connection_options_.host = host_;
        connection_options_.port = port_;
        connection_options_.db = db_index_;
        
        // TLS 설정 적용
        if (tlsOptions_.enabled())
        {
            connection_options_.tls.enabled = true;
            connection_options_.tls.cert = tlsOptions_.cert_path();
            connection_options_.tls.key = tlsOptions_.key_path();
            connection_options_.tls.cacert = tlsOptions_.ca_cert_path();
        }
    }

    RedisConnector::~RedisConnector()
    {
        disconnect();
    }

    auto RedisConnector::connect() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            redis_ = std::make_shared<sw::redis::Redis>(connection_options_);
            
            // 연결 테스트
            redis_->ping();
            is_connected_ = true;
            
            Logger::handle().write(LogTypes::Information,
                "Redis connector established connection to " + host_ + ":" + std::to_string(port_));
            
            return { true, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            is_connected_ = false;
            return { false, std::string("Redis connection error: ") + e.what() };
        }
    }

    auto RedisConnector::disconnect() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (redis_)
            {
                redis_.reset();
            }
            if (transaction_)
            {
                transaction_.reset();
            }
            is_connected_ = false;
            
            Logger::handle().write(LogTypes::Information,
                "Redis connector disconnected from " + host_ + ":" + std::to_string(port_));
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Redis disconnection error: ") + e.what() };
        }
    }

    auto RedisConnector::is_connected() const -> bool
    {
        return is_connected_ && redis_ != nullptr;
    }

    auto RedisConnector::ping() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            redis_->ping();
            return { true, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("Ping failed: ") + e.what() };
        }
    }

    auto RedisConnector::select_db(int db_index) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            redis_->command("SELECT", std::to_string(db_index));
            db_index_ = db_index;
            return { true, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("SELECT DB failed: ") + e.what() };
        }
    }

    // Basic operations
    auto RedisConnector::set(const std::string& key, const std::string& value, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            if (ttl_sec > 0)
            {
                redis_->setex(key, ttl_sec, value);
            }
            else
            {
                redis_->set(key, value);
            }
            
            return { true, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("SET failed: ") + e.what() };
        }
    }

    auto RedisConnector::get(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { "", "Not connected to Redis" };
            }
            
            auto val = redis_->get(key);
            if (val)
            {
                return { *val, std::nullopt };
            }
            else
            {
                return { "", "Key not found" };
            }
        }
        catch (const sw::redis::Error& e)
        {
            return { "", std::string("GET failed: ") + e.what() };
        }
    }

    auto RedisConnector::del(const std::string& key) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            auto deleted = redis_->del(key);
            return { deleted > 0, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("DEL failed: ") + e.what() };
        }
    }

    auto RedisConnector::exists(const std::string& key) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            auto count = redis_->exists(key);
            return { count > 0, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("EXISTS failed: ") + e.what() };
        }
    }

    auto RedisConnector::expire(const std::string& key, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            auto result = redis_->expire(key, ttl_sec);
            return { result, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("EXPIRE failed: ") + e.what() };
        }
    }

    auto RedisConnector::ttl(const std::string& key) -> std::tuple<int64_t, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { -2, "Not connected to Redis" };
            }
            
            auto ttl_value = redis_->ttl(key);
            return { ttl_value, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { -2, std::string("TTL failed: ") + e.what() };
        }
    }

    // Hash operations
    auto RedisConnector::hset(const std::string& key, const std::string& field, const std::string& value) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Not connected to Redis" };
            }
            
            redis_->hset(key, field, value);
            return { true, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { false, std::string("HSET failed: ") + e.what() };
        }
    }

    auto RedisConnector::hget(const std::string& key, const std::string& field) -> std::tuple<std::string, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { "", "Not connected to Redis" };
            }
            
            auto val = redis_->hget(key, field);
            if (val)
            {
                return { *val, std::nullopt };
            }
            else
            {
                return { "", "Field not found" };
            }
        }
        catch (const sw::redis::Error& e)
        {
            return { "", std::string("HGET failed: ") + e.what() };
        }
    }

    auto RedisConnector::hgetall(const std::string& key) -> std::tuple<std::unordered_map<std::string, std::string>, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { {}, "Not connected to Redis" };
            }
            
            std::unordered_map<std::string, std::string> result;
            redis_->hgetall(key, std::inserter(result, result.begin()));
            return { result, std::nullopt };
        }
        catch (const sw::redis::Error& e)
        {
            return { {}, std::string("HGETALL failed: ") + e.what() };
        }
    }

    // Getters
    auto RedisConnector::get_redis() const -> std::shared_ptr<sw::redis::Redis>
    {
        return redis_;
    }

    auto RedisConnector::get_transaction() const -> std::shared_ptr<sw::redis::Transaction>
    {
        return transaction_;
    }
}
