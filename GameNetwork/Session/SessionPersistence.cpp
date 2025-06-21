#include "SessionPersistence.h"
#include "Character.h"

#include <sstream>
#include <iomanip>

namespace GameNetwork
{
    SessionPersistence::SessionPersistence(std::shared_ptr<Redis::RedisClient> redis_client)
        : redis_client_(redis_client)
        , key_prefix_(DEFAULT_KEY_PREFIX)
        , auto_save_enabled_(false)
        , auto_save_interval_(DEFAULT_AUTO_SAVE_INTERVAL)
        , compression_enabled_(true)
        , auto_save_running_(false)
    {
        stats_ = {};
    }
    
    SessionPersistence::~SessionPersistence()
    {
        if (auto_save_running_)
        {
            auto_save_running_ = false;
            if (auto_save_thread_.valid())
            {
                auto_save_thread_.wait();
            }
        }
    }
    
    auto SessionPersistence::save_session(std::shared_ptr<GameSession> session) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session)
        {
            return {false, "Null session provided"};
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        try
        {
            // Serialize session
            auto [data, error] = serialize_session(session);
            if (error.has_value())
            {
                stats_.save_failures++;
                return {false, error};
            }
            
            // Compress if enabled
            std::vector<uint8_t> final_data = data;
            if (compression_enabled_)
            {
                final_data = compress_data(data);
                stats_.compression_ratio_percent = 
                    (data.size() > 0) ? (final_data.size() * 100 / data.size()) : 100;
            }
            
            // Save to Redis
            std::string key = get_session_key(session->session_id());
            auto [success, set_error] = redis_client_->set(key, 
                std::string(final_data.begin(), final_data.end()));
            
            if (!success)
            {
                stats_.save_failures++;
                return {false, set_error.value_or("Failed to save to Redis")};
            }
            
            // Update indices
            update_indices(session->session_id(), 
                           session->account_id(), 
                           ""); // TODO: Get actual server ID
            
            // Set TTL
            redis_client_->set_ttl(key, 
                static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(DEFAULT_SESSION_TTL).count()));
            
            // Update stats
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            stats_.sessions_saved++;
            stats_.average_save_time_ms = 
                (stats_.average_save_time_ms * (stats_.sessions_saved - 1) + 
                 duration.count()) / stats_.sessions_saved;
            stats_.total_storage_bytes += final_data.size();
            
            return {true, std::nullopt};
        }
        catch (const std::exception& e)
        {
            stats_.save_failures++;
            return {false, std::string("Exception during save: ") + e.what()};
        }
    }
    
    auto SessionPersistence::load_session(const std::string& session_id) 
        -> std::tuple<std::optional<SessionSnapshot>, std::optional<std::string>>
    {
        auto start_time = std::chrono::steady_clock::now();
        
        try
        {
            std::string key = get_session_key(session_id);
            auto [data, get_error] = redis_client_->get(key);
            
            if (get_error.has_value())
            {
                stats_.load_failures++;
                return {std::nullopt, get_error.value()};
            }
            
            // Convert to vector
            std::vector<uint8_t> binary_data(data.begin(), data.end());
            
            // Decompress if needed
            if (compression_enabled_)
            {
                binary_data = decompress_data(binary_data);
            }
            
            // Deserialize
            auto [snapshot, error] = deserialize_session(binary_data);
            if (error.has_value())
            {
                stats_.load_failures++;
                return {std::nullopt, error};
            }
            
            // Update stats
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            stats_.sessions_loaded++;
            stats_.average_load_time_ms = 
                (stats_.average_load_time_ms * (stats_.sessions_loaded - 1) + 
                 duration.count()) / stats_.sessions_loaded;
            
            return {snapshot, std::nullopt};
        }
        catch (const std::exception& e)
        {
            stats_.load_failures++;
            return {std::nullopt, std::string("Exception during load: ") + e.what()};
        }
    }
    
    auto SessionPersistence::serialize_session(std::shared_ptr<GameSession> session) const 
        -> std::tuple<std::vector<uint8_t>, std::optional<std::string>>
    {
        try
        {
            // Create snapshot
            SessionSnapshot snapshot;
            snapshot.session_id = session->session_id();
            snapshot.account_id = session->account_id();
            snapshot.state = session->state();
            snapshot.location = session->current_location();
            snapshot.created_time = std::chrono::system_clock::now();
            snapshot.last_save_time = std::chrono::system_clock::now();
            snapshot.version = 1;
            
            // Serialize character if exists
            auto character = session->current_character();
            if (character)
            {
                snapshot.character_data = character->serialize();
            }
            
            // TODO: Serialize custom data
            // This would use a proper serialization library like protobuf or msgpack
            
            // For now, simple binary format
            std::vector<uint8_t> data;
            
            // Write version
            data.insert(data.end(), 
                        reinterpret_cast<const uint8_t*>(&snapshot.version),
                        reinterpret_cast<const uint8_t*>(&snapshot.version) + sizeof(uint32_t));
            
            // Write session ID length and data
            uint32_t id_len = static_cast<uint32_t>(snapshot.session_id.size());
            data.insert(data.end(), 
                        reinterpret_cast<const uint8_t*>(&id_len),
                        reinterpret_cast<const uint8_t*>(&id_len) + sizeof(uint32_t));
            data.insert(data.end(), 
                        snapshot.session_id.begin(), 
                        snapshot.session_id.end());
            
            // Write account ID
            uint32_t acc_len = static_cast<uint32_t>(snapshot.account_id.size());
            data.insert(data.end(), 
                        reinterpret_cast<const uint8_t*>(&acc_len),
                        reinterpret_cast<const uint8_t*>(&acc_len) + sizeof(uint32_t));
            data.insert(data.end(), 
                        snapshot.account_id.begin(), 
                        snapshot.account_id.end());
            
            // Write state
            uint8_t state = static_cast<uint8_t>(snapshot.state);
            data.push_back(state);
            
            // Write location
            data.insert(data.end(), 
                        reinterpret_cast<const uint8_t*>(&snapshot.location),
                        reinterpret_cast<const uint8_t*>(&snapshot.location) + sizeof(Location));
            
            // Write character data size and data
            uint32_t char_size = static_cast<uint32_t>(snapshot.character_data.size());
            data.insert(data.end(), 
                        reinterpret_cast<const uint8_t*>(&char_size),
                        reinterpret_cast<const uint8_t*>(&char_size) + sizeof(uint32_t));
            data.insert(data.end(), 
                        snapshot.character_data.begin(), 
                        snapshot.character_data.end());
            
            return {data, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {{}, std::string("Serialization error: ") + e.what()};
        }
    }
    
    auto SessionPersistence::deserialize_session(const std::vector<uint8_t>& data) const 
        -> std::tuple<std::optional<SessionSnapshot>, std::optional<std::string>>
    {
        try
        {
            if (data.size() < sizeof(uint32_t))
            {
                return {std::nullopt, "Data too small"};
            }
            
            SessionSnapshot snapshot;
            size_t offset = 0;
            
            // Read version
            std::memcpy(&snapshot.version, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            // Read session ID
            uint32_t id_len;
            std::memcpy(&id_len, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            snapshot.session_id.assign(
                reinterpret_cast<const char*>(data.data() + offset), id_len);
            offset += id_len;
            
            // Read account ID
            uint32_t acc_len;
            std::memcpy(&acc_len, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            snapshot.account_id.assign(
                reinterpret_cast<const char*>(data.data() + offset), acc_len);
            offset += acc_len;
            
            // Read state
            uint8_t state = data[offset++];
            snapshot.state = static_cast<SessionState>(state);
            
            // Read location
            std::memcpy(&snapshot.location, data.data() + offset, sizeof(Location));
            offset += sizeof(Location);
            
            // Read character data
            uint32_t char_size;
            std::memcpy(&char_size, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            if (char_size > 0)
            {
                snapshot.character_data.resize(char_size);
                std::memcpy(snapshot.character_data.data(), 
                            data.data() + offset, char_size);
            }
            
            return {snapshot, std::nullopt};
        }
        catch (const std::exception& e)
        {
            return {std::nullopt, std::string("Deserialization error: ") + e.what()};
        }
    }
    
    auto SessionPersistence::compress_data(const std::vector<uint8_t>& data) const 
        -> std::vector<uint8_t>
    {
        // TODO: Implement actual compression (zlib, lz4, etc.)
        // For now, return original data
        return data;
    }
    
    auto SessionPersistence::decompress_data(const std::vector<uint8_t>& compressed) const 
        -> std::vector<uint8_t>
    {
        // TODO: Implement actual decompression
        // For now, return original data
        return compressed;
    }
    
    auto SessionPersistence::get_session_key(const std::string& session_id) const -> std::string
    {
        return key_prefix_ + "data:" + session_id;
    }
    
    auto SessionPersistence::get_account_index_key(const std::string& account_id) const -> std::string
    {
        return key_prefix_ + "index:account:" + account_id;
    }
    
    auto SessionPersistence::get_lock_key(const std::string& session_id) const -> std::string
    {
        return key_prefix_ + "lock:" + session_id;
    }
    
    auto SessionPersistence::update_indices(const std::string& session_id,
                                            const std::string& account_id,
                                            const std::string& server_id) -> void
    {
        // Update account index
        std::string account_key = get_account_index_key(account_id);
        redis_client_->set(account_key, session_id);
        redis_client_->set_ttl(account_key, 
            static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(DEFAULT_SESSION_TTL).count()));
        
        // TODO: Redis集合操作 (sadd) 暂时注释，需要扩展RedisClient API
        // Update server index if provided
        // if (!server_id.empty())
        // {
        //     std::string server_key = key_prefix_ + "index:server:" + server_id;
        //     redis_client_->sadd(server_key, session_id);
        // }
        
        // Update active sessions set
        // std::string active_key = key_prefix_ + "active";
        // redis_client_->sadd(active_key, session_id);
    }
    
    auto SessionPersistence::session_exists(const std::string& session_id) const -> bool
    {
        std::string key = get_session_key(session_id);
        // TODO: 使用get方法检查key是否存在，因为RedisClient没有exists方法
        auto [data, error] = redis_client_->get(key);
        return !error.has_value();
    }
    
    auto SessionPersistence::mark_session_active(const std::string& session_id,
                                                 const std::string& server_id) -> void
    {
        // TODO: Redis集合操作暂时注释
        // std::string active_key = key_prefix_ + "active";
        // redis_client_->sadd(active_key, session_id);
        
        // if (!server_id.empty())
        // {
        //     std::string server_key = key_prefix_ + "index:server:" + server_id;
        //     redis_client_->sadd(server_key, session_id);
        // }
    }
    
    auto SessionPersistence::mark_session_inactive(const std::string& session_id) -> void
    {
        // TODO: Redis集合操作暂时注释
        // std::string active_key = key_prefix_ + "active";
        // redis_client_->srem(active_key, session_id);
    }
    
    auto SessionPersistence::acquire_session_lock(const std::string& session_id,
                                                  const std::string& holder_id,
                                                  std::chrono::seconds timeout) -> bool
    {
        std::string lock_key = get_lock_key(session_id);
        
        // 简化的锁实现，只使用基本的set操作
        auto [success, error] = redis_client_->set(lock_key, holder_id, 
            static_cast<uint32_t>(timeout.count()));
        return success;
    }
    
    auto SessionPersistence::release_session_lock(const std::string& session_id,
                                                  const std::string& holder_id) -> bool
    {
        std::string lock_key = get_lock_key(session_id);
        
        // 检查是否拥有锁
        auto [current_holder, error] = redis_client_->get(lock_key);
        if (!error.has_value() && current_holder == holder_id)
        {
            // TODO: RedisClient没有del方法，使用set空值代替
            redis_client_->set(lock_key, "", 1); // 1秒后过期
            return true;
        }
        
        return false;
    }
    
    auto SessionPersistence::enable_auto_save(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (enable && !auto_save_enabled_)
        {
            auto_save_enabled_ = true;
            auto_save_running_ = true;
            
            auto_save_thread_ = std::async(std::launch::async, [this]()
            {
                while (auto_save_running_)
                {
                    std::this_thread::sleep_for(auto_save_interval_);
                    
                    // TODO: Implement auto-save logic
                    // This would iterate through active sessions and save them
                }
            });
        }
        else if (!enable && auto_save_enabled_)
        {
            auto_save_enabled_ = false;
            auto_save_running_ = false;
            
            if (auto_save_thread_.valid())
            {
                auto_save_thread_.wait();
            }
        }
    }
    
    auto SessionPersistence::get_stats() const -> PersistenceStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    auto SessionPersistence::reset_stats() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = {};
    }
}