#include "RedisClient.h"
#include <Logger.h>
#include <Converter.h>

using namespace Utilities;

namespace Redis
{
    RedisClient::RedisClient(const std::string& host, const int& port, const TLSOptions& tls_options, const int& db_index)
        : redis_connector_(std::make_shared<RedisConnector>(host, port, tls_options, db_index))
    {
    }

    RedisClient::~RedisClient()
    {
        disconnect();
    }

    auto RedisClient::connect() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (is_connected())
            {
                return { true, std::nullopt };
            }

            auto [success, error] = redis_connector_->connect();
            if (!success)
            {
                return { false, error };
            }

            Logger::handle().write(LogTypes::Information, 
                "Redis client connected successfully");
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Redis connection failed: ") + e.what() };
        }
    }

    auto RedisClient::is_connected() const -> bool
    {
        return redis_connector_->is_connected();
    }

    auto RedisClient::disconnect() -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { true, std::nullopt };
            }

            redis_connector_->disconnect();
            
            Logger::handle().write(LogTypes::Information, 
                "Redis client disconnected");
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Redis disconnection failed: ") + e.what() };
        }
    }

    auto RedisClient::set(const std::string& key, const std::string& value, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Redis client not connected" };
            }

            auto [success, error] = redis_connector_->set(key, value, ttl_sec);
            if (!success)
            {
                return { false, error };
            }

            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Redis SET failed: ") + e.what() };
        }
    }

    auto RedisClient::get(const std::string& key) -> std::tuple<std::string, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { "", "Redis client not connected" };
            }

            auto [value, error] = redis_connector_->get(key);
            if (error.has_value())
            {
                return { "", error };
            }

            return { value, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { "", std::string("Redis GET failed: ") + e.what() };
        }
    }

    auto RedisClient::set_ttl(const std::string& key, std::uint32_t ttl_sec) -> std::tuple<bool, std::optional<std::string>>
    {
        try
        {
            if (!is_connected())
            {
                return { false, "Redis client not connected" };
            }

            auto [success, error] = redis_connector_->expire(key, ttl_sec);
            if (!success)
            {
                return { false, error };
            }

            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Redis EXPIRE failed: ") + e.what() };
        }
    }
}
