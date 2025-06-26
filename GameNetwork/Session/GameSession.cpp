#include "GameSession.h"
#include "../Synchronization/Character.h"
#include "SessionPersistence.h"
#include "../../Utilities/Logger.h"
#include "../../Utilities/Converter.h"
#include <functional>

namespace GameNetwork
{
    GameSession::GameSession(const std::string& session_id, const std::string& account_id)
        : session_id_(session_id)
        , account_id_(account_id)
        , state_(SessionConnectionState::Connected)
        , channel_id_(0)
        , last_activity_(std::chrono::steady_clock::now())
        , kicked_by_duplicate_login_(false)
    {
        current_location_.x = 0.0f;
        current_location_.y = 0.0f;
        current_location_.z = 0.0f;
        current_location_.map_id = 0;
        current_location_.zone_id = 0;
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
            // Use static load method from Character class
            auto [loaded_character, error] = Character::load(character_id, account_id_);
            if (!loaded_character)
            {
                return { false, error.value_or("Failed to load character") };
            }
            
            character_ = loaded_character;
            
            // Set character's initial location
            current_location_ = character_->get_location();
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
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
        
        if (!character_)
        {
            return { false, "No character loaded" };
        }
        
        try
        {
            // Update character location before saving
            character_->set_location(current_location_);
            
            auto [success, error] = character_->save();
            if (!success)
            {
                return { false, error };
            }
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Failed to save character: ") + e.what() };
        }
    }

    auto GameSession::current_location() const -> Location
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_location_;
    }

    auto GameSession::location() const -> Location
    {
        return current_location();
    }

    auto GameSession::move_to(const Location& location) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_location_ = location;
        
        if (character_)
        {
            character_->set_location(location);
        }
    }

    auto GameSession::teleport_to(const Location& location) -> void
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
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Session " + session_id_ + " entered channel " + std::to_string(channel_id));
        
        return { true, std::nullopt };
    }

    auto GameSession::leave_channel() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (channel_id_ != 0)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
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
            data.character_id = character_ ? character_->id : 0;
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

    template<typename T>
    auto GameSession::set_data(const std::string& key, const T& value) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        custom_data_[key] = value;
    }

    template<typename T>
    auto GameSession::get_data(const std::string& key) const -> std::optional<T>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = custom_data_.find(key);
        if (it == custom_data_.end())
        {
            return std::nullopt;
        }
        
        try
        {
            return std::any_cast<T>(it->second);
        }
        catch (const std::bad_any_cast&)
        {
            return std::nullopt;
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
                
                Utilities::Logger::handle().write(Utilities::LogTypes::Information,
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

    auto GameSession::get_entity_id() const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (character_)
        {
            // Assuming Character has an id() method
            // return character_->id();
            return 0; // Placeholder until Character class is fully implemented
        }
        return 0;
    }

    auto GameSession::send_packet(const std::vector<uint8_t>& packet_data) -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connection_ && is_online())
        {
            return connection_->send_binary(packet_data);
        }
        return false;
    }

    // Template instantiations
    template void GameSession::set_data<int>(const std::string&, const int&);
    template void GameSession::set_data<uint64_t>(const std::string&, const uint64_t&);
    template void GameSession::set_data<float>(const std::string&, const float&);
    template void GameSession::set_data<double>(const std::string&, const double&);
    template void GameSession::set_data<std::string>(const std::string&, const std::string&);
    template void GameSession::set_data<bool>(const std::string&, const bool&);

    template std::optional<int> GameSession::get_data<int>(const std::string&) const;
    template std::optional<uint64_t> GameSession::get_data<uint64_t>(const std::string&) const;
    template std::optional<float> GameSession::get_data<float>(const std::string&) const;
    template std::optional<double> GameSession::get_data<double>(const std::string&) const;
    template std::optional<std::string> GameSession::get_data<std::string>(const std::string&) const;
    template std::optional<bool> GameSession::get_data<bool>(const std::string&) const;
    
    // Security token management
    auto GameSession::set_session_token(const std::string& token) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_token_ = token;
    }
    
    auto GameSession::get_session_token() const -> std::string
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return session_token_;
    }
    
    auto GameSession::clear_session_token() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_token_.clear();
    }
    
    // Session limits
    auto GameSession::set_kicked_by_duplicate_login(bool kicked) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        kicked_by_duplicate_login_ = kicked;
    }
    
    auto GameSession::was_kicked_by_duplicate_login() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return kicked_by_duplicate_login_;
    }
}