#include "GameSession.h"
// Character.h는 필요시에만 include하도록 제거
#include "SessionPersistence.h"
#include "../../Utilities/Logger.h"
#include "../../Utilities/Converter.h"
#include <functional>

using namespace Utilities;

namespace GameNetwork
{
    GameSession::GameSession(const std::string& session_id, const std::string& account_id)
        : session_id_(session_id)
        , account_id_(account_id)
        , state_(SessionConnectionState::Connected)
        , channel_id_(0)
        , last_activity_(std::chrono::steady_clock::now())
        , current_server_id_("unassigned")
        , kicked_by_duplicate_login_(false)
        , session_recording_enabled_(false)
        , packets_sent_count_(0)
        , packets_received_count_(0)
        , bytes_sent_count_(0)
        , bytes_received_count_(0)
        , reconnection_count_(0)
        , command_count_(0)
    {
        current_location_ = 0;  // Default location ID
        created_time_ = std::chrono::steady_clock::now();
    }

    GameSession::~GameSession()
    {
        unbind_connection();
        stop_grace_period_timer();
    }

    auto GameSession::id() const -> uint64_t
    {
        return session_id_hash();
    }

    auto GameSession::session_id() const -> std::string
    {
        return session_id_;
    }

    auto GameSession::session_id_hash() const -> uint64_t
    {
        std::hash<std::string> hasher;
        return hasher(session_id_);
    }

    auto GameSession::account_id() const -> std::string
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return account_id_;
    }

    auto GameSession::set_account_id(const std::string& id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        account_id_ = id;
    }

    auto GameSession::bind_connection(std::shared_ptr<GameConnection> connection) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connection_)
        {
            connection_->unbind_session();
        }
        
        connection_ = connection;
        if (connection_)
        {
            connection_->bind_session(shared_from_this());
            state_ = SessionConnectionState::Connected;
            stop_grace_period_timer();
        }
        
        update_last_activity();
    }

    auto GameSession::unbind_connection() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connection_)
        {
            connection_->unbind_session();
            connection_.reset();
            state_ = SessionConnectionState::Disconnected;
            start_grace_period_timer();
        }
    }

    auto GameSession::current_connection() const -> std::shared_ptr<GameConnection>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    auto GameSession::connection() const -> std::shared_ptr<GameConnection>
    {
        return current_connection();
    }

    auto GameSession::is_online() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_ != nullptr && state_ == SessionConnectionState::Connected;
    }

    auto GameSession::state() const -> SessionConnectionState
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    auto GameSession::set_state(SessionConnectionState new_state) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = new_state;
    }

    auto GameSession::load_character(uint64_t character_id) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try
        {
            // Character loading is disabled for now - would need proper interface
            /*
            auto [loaded_character, error] = Character::load(character_id, account_id_);
            if (!loaded_character)
            {
                return { false, error.value_or("Failed to load character") };
            }
            
            character_ = loaded_character;
            
            // Set character's initial location
            current_location_ = character_->get_location();
            */
            
            // For now, just return success
            return { true, std::nullopt };
            
            Logger::handle().write(LogTypes::Information,
                "Character loaded for session " + session_id_ + ", character ID: " + std::to_string(character_id));
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            character_.reset();
            return { false, std::string("Failed to load character: ") + e.what() };
        }
    }

    auto GameSession::current_character() const -> std::shared_ptr<Character>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return character_;
    }

    auto GameSession::save_character() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        /*
        if (!character_)
        {
            return { false, "No character loaded" };
        }
        */
        
        try
        {
            /*
            // Update character location before saving
            character_->set_location(current_location_);
            
            auto [success, error] = character_->save();
            if (!success)
            {
                return { false, error };
            }
            */
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Failed to save character: ") + e.what() };
        }
    }

    auto GameSession::current_location() const -> int
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_location_;
    }

    auto GameSession::location() const -> int
    {
        return current_location();
    }

    auto GameSession::move_to(int location) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_location_ = location;
        
        /*
        if (character_)
        {
            character_->set_location(location);
        }
        */
    }

    auto GameSession::teleport_to(int location) -> void
    {
        move_to(location);
        
        // Additional teleport-specific logic could go here
        // For example, clearing movement buffers, notifying nearby players, etc.
    }

    auto GameSession::enter_channel(uint32_t channel_id) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (channel_id_ != 0)
        {
            return { false, "Already in channel " + std::to_string(channel_id_) };
        }
        
        channel_id_ = channel_id;
        
        Logger::handle().write(LogTypes::Information,
            "Session " + session_id_ + " entered channel " + std::to_string(channel_id));
        
        return { true, std::nullopt };
    }

    auto GameSession::leave_channel() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (channel_id_ != 0)
        {
            Logger::handle().write(LogTypes::Information,
                "Session " + session_id_ + " left channel " + std::to_string(channel_id_));
            channel_id_ = 0;
        }
    }

    auto GameSession::current_channel_id() const -> uint32_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return channel_id_;
    }

    auto GameSession::update_last_activity() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_activity_ = std::chrono::steady_clock::now();
    }

    auto GameSession::last_activity_time() const -> std::chrono::steady_clock::time_point
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_activity_;
    }

    auto GameSession::is_timeout() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != SessionConnectionState::Disconnected)
        {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_);
        
        return elapsed > kSessionTimeout;
    }

    auto GameSession::remaining_grace_period() const -> std::chrono::seconds
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != SessionConnectionState::Disconnected)
        {
            return std::chrono::seconds(0);
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_);
        
        if (elapsed >= kSessionGracePeriod)
        {
            return std::chrono::seconds(0);
        }
        
        return kSessionGracePeriod - elapsed;
    }

    auto GameSession::save_state() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try
        {
            SessionPersistence persistence;
            
            // Prepare session data
            SessionData data;
            data.session_id = session_id_;
            data.account_id = account_id_;
            data.character_id = 0; // character_ ? character_->id : 0;
            data.location = current_location_;
            data.channel_id = channel_id_;
            data.last_activity = last_activity_;
            data.custom_data = custom_data_;
            
            auto [success, error] = persistence.save_session(session_id_, data);
            if (!success)
            {
                return { false, error };
            }
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Failed to save session state: ") + e.what() };
        }
    }

    auto GameSession::restore_state() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try
        {
            SessionPersistence persistence;
            auto [data, error] = persistence.load_session(session_id_);
            
            if (error.has_value())
            {
                return { false, error };
            }
            
            // Restore session data
            account_id_ = data.account_id;
            current_location_ = data.location;
            channel_id_ = data.channel_id;
            last_activity_ = data.last_activity;
            custom_data_ = data.custom_data;
            
            // Restore character if needed
            if (data.character_id > 0)
            {
                auto [char_success, char_error] = load_character(data.character_id);
                if (!char_success)
                {
                    return { false, char_error };
                }
            }
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Failed to restore session state: ") + e.what() };
        }
    }

    auto GameSession::remove_data(const std::string& key) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        custom_data_.erase(key);
    }

    auto GameSession::start_grace_period_timer() -> void
    {
        if (grace_timer_.valid())
        {
            return;
        }
        
        grace_timer_ = std::async(std::launch::async, [this]()
        {
            std::this_thread::sleep_for(kSessionGracePeriod);
            
            // Check if still disconnected after grace period
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == SessionConnectionState::Disconnected && !connection_)
            {
                state_ = SessionConnectionState::Expired;
                
                Logger::handle().write(LogTypes::Information,
                    "Session " + session_id_ + " expired after grace period");
            }
        });
    }

    auto GameSession::stop_grace_period_timer() -> void
    {
        if (grace_timer_.valid())
        {
            // Note: We can't cancel std::async, it will run to completion
            // But the state check in the timer will prevent any action
        }
    }

    auto GameSession::get_account_id() const -> std::string
    {
        return account_id();
    }

    auto GameSession::get_channel_id() const -> uint32_t
    {
        return current_channel_id();
    }

    auto GameSession::is_connected() const -> bool
    {
        return is_online();
    }

    auto GameSession::is_active() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == SessionConnectionState::Connected && connection_ != nullptr;
    }

    auto GameSession::get_character_id() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return character_id_;
    }

    auto GameSession::set_character_id(uint64_t character_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        character_id_ = character_id;
    }

    auto GameSession::get_entity_id() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return entity_id_;
    }

    auto GameSession::set_entity_id(uint64_t entity_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entity_id_ = entity_id;
    }

    auto GameSession::set_current_server_id(const std::string& server_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_server_id_ = server_id;
        Logger::handle().write(LogTypes::Information, 
            "Session " + session_id_ + " assigned to server: " + server_id);
    }

    auto GameSession::get_player_location() const -> std::optional<int>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (character_)
        {
            return current_location_;
        }
        return std::nullopt;
    }

    auto GameSession::set_player_location(int location_id) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_location_ = location_id;
    }

    auto GameSession::get_all_custom_data() const -> const std::unordered_map<std::string, std::any>&
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return custom_data_;
    }

    // ⭐ Session Statistics and Metadata Implementation
    auto GameSession::get_session_statistics() -> SessionStatistics
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        SessionStatistics stats;
        stats.creation_time = created_time_;
        stats.last_activity_time = last_activity_;
        stats.total_session_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - created_time_);
        stats.packets_sent = packets_sent_count_;
        stats.packets_received = packets_received_count_;
        stats.bytes_sent = bytes_sent_count_;
        stats.bytes_received = bytes_received_count_;
        stats.reconnection_count = reconnection_count_;
        stats.command_count = command_count_;
        stats.activity_log = activity_log_;
        
        return stats;
    }

    auto GameSession::set_session_metadata(const std::unordered_map<std::string, std::string>& metadata) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_metadata_ = metadata;
        
        if (session_recording_enabled_)
        {
            add_activity_log("Metadata updated with " + std::to_string(metadata.size()) + " entries");
        }
    }

    auto GameSession::get_session_metadata() const -> std::unordered_map<std::string, std::string>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return session_metadata_;
    }

    auto GameSession::enable_session_recording(bool enable) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_recording_enabled_ = enable;
        
        if (enable)
        {
            add_activity_log("Session recording enabled");
        }
        else
        {
            add_activity_log("Session recording disabled");
        }
    }

    auto GameSession::is_session_recording_enabled() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return session_recording_enabled_;
    }

    auto GameSession::add_activity_log(const std::string& activity) -> void
    {
        if (!session_recording_enabled_) return;
        
        auto now = std::chrono::steady_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::string log_entry = "[" + std::to_string(timestamp) + "] " + activity;
        activity_log_.push_back(log_entry);
        
        // Maintain max log size
        if (activity_log_.size() > MAX_ACTIVITY_LOG_SIZE)
        {
            activity_log_.erase(activity_log_.begin());
        }
    }

    auto GameSession::clear_activity_log() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activity_log_.clear();
        
        if (session_recording_enabled_)
        {
            add_activity_log("Activity log cleared");
        }
    }
}