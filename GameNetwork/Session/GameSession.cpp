#include "GameSession.h"
#include "Character.h"

#include <future>

namespace GameNetwork
{
    GameSession::GameSession(const std::string& session_id, const std::string& account_id)
        : session_id_(session_id)
        , account_id_(account_id)
        , state_(SessionState::Inactive)
        , created_time_(std::chrono::steady_clock::now())
        , last_activity_(std::chrono::steady_clock::now())
    {
        // Initialize location
        current_location_ = {0.0f, 0.0f, 0.0f, 0, 0};
    }
    
    GameSession::~GameSession()
    {
        stop_grace_period_timer();
        
        // Save state before destruction
        save_state();
    }
    
    auto GameSession::session_id() const -> std::string
    {
        return session_id_;
    }
    
    auto GameSession::account_id() const -> std::string
    {
        return account_id_;
    }
    
    auto GameSession::bind_connection(std::shared_ptr<GameConnection> connection) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        connection_ = connection;
        state_ = SessionState::Active;
        update_last_activity();
        
        stop_grace_period_timer();
        
        // Setup packet handler
        if (connection_)
        {
            connection_->register_packet_handler(
                [this](const GamePacket& packet)
                {
                    // Handle incoming packets
                    // This would be dispatched to MessageDispatcher
                });
        }
    }
    
    auto GameSession::unbind_connection() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connection_)
        {
            connection_.reset();
            disconnected_time_ = std::chrono::steady_clock::now();
            
            // Start grace period timer for reconnection
            start_grace_period_timer();
        }
    }
    
    auto GameSession::current_connection() const -> std::shared_ptr<GameConnection>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }
    
    auto GameSession::is_online() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_ != nullptr && connection_->is_connected();
    }
    
    auto GameSession::state() const -> SessionState
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }
    
    auto GameSession::set_state(SessionState new_state) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = new_state;
    }
    
    auto GameSession::load_character(uint64_t character_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // TODO: Load character from database
        // For now, create a dummy character
        character_ = std::make_shared<Character>(character_id, "Player_" + std::to_string(character_id));
        character_->set_level(1);
        character_->set_health(100);
        character_->set_max_health(100);
        character_->set_mana(50);
        character_->set_max_mana(50);
        character_->set_location(current_location_);
        
        return {true, std::nullopt};
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
            return {false, "No character loaded"};
        }
        
        // TODO: Save character to database
        
        return {true, std::nullopt};
    }
    
    auto GameSession::current_location() const -> Location
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_location_;
    }
    
    auto GameSession::move_to(const Location& location) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        current_location_ = location;
        
        if (character_)
        {
            character_->set_location(location);
        }
        
        update_last_activity();
    }
    
    auto GameSession::teleport_to(const Location& location) -> void
    {
        move_to(location);
        
        // TODO: Send teleport packet to client
    }
    
    auto GameSession::enter_channel(uint32_t channel_id) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (channel_id == INVALID_CHANNEL_ID)
        {
            return {false, "Invalid channel ID"};
        }
        
        current_location_.channel_id = channel_id;
        
        if (character_)
        {
            character_->set_location(current_location_);
        }
        
        return {true, std::nullopt};
    }
    
    auto GameSession::leave_channel() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        current_location_.channel_id = INVALID_CHANNEL_ID;
        
        if (character_)
        {
            character_->set_location(current_location_);
        }
    }
    
    auto GameSession::current_channel_id() const -> uint32_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_location_.channel_id;
    }
    
    auto GameSession::save_state() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // TODO: Implement actual state persistence
        // This would save to Redis or database:
        // - Session info
        // - Character data
        // - Location
        // - Custom session data
        
        // Save character first
        if (character_)
        {
            auto [success, error] = save_character();
            if (!success)
            {
                return {false, error};
            }
        }
        
        return {true, std::nullopt};
    }
    
    auto GameSession::restore_state() -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // TODO: Implement actual state restoration
        // This would load from Redis or database
        
        return {true, std::nullopt};
    }
    
    auto GameSession::update_last_activity() -> void
    {
        last_activity_ = std::chrono::steady_clock::now();
    }
    
    auto GameSession::last_activity_time() const -> std::chrono::steady_clock::time_point
    {
        return last_activity_;
    }
    
    auto GameSession::is_timeout() const -> bool
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_);
        return elapsed > SESSION_TIMEOUT;
    }
    
    auto GameSession::remaining_grace_period() const -> std::chrono::seconds
    {
        if (!is_online())
        {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - disconnected_time_);
            auto remaining = RECONNECT_GRACE_PERIOD - elapsed;
            
            return remaining.count() > 0 ? remaining : std::chrono::seconds(0);
        }
        
        return std::chrono::seconds(0);
    }
    
    template<typename T>
    auto GameSession::set_data(const std::string& key, const T& value) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_data_[key] = value;
    }
    
    template<typename T>
    auto GameSession::get_data(const std::string& key) const -> std::optional<T>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = session_data_.find(key);
        if (it != session_data_.end())
        {
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch (const std::bad_any_cast&)
            {
                return std::nullopt;
            }
        }
        
        return std::nullopt;
    }
    
    auto GameSession::remove_data(const std::string& key) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_data_.erase(key);
    }
    
    auto GameSession::start_grace_period_timer() -> void
    {
        grace_period_timer_ = std::async(std::launch::async, [this]()
        {
            std::this_thread::sleep_for(RECONNECT_GRACE_PERIOD);
            
            // Check if still disconnected after grace period
            if (!is_online())
            {
                set_state(SessionState::Terminating);
            }
        });
    }
    
    auto GameSession::stop_grace_period_timer() -> void
    {
        if (grace_period_timer_.valid())
        {
            // Note: Can't cancel std::async, it will complete
            // but the check in the lambda will prevent action
        }
    }
    
    // Explicit template instantiations
    template auto GameSession::set_data<int>(const std::string&, const int&) -> void;
    template auto GameSession::set_data<float>(const std::string&, const float&) -> void;
    template auto GameSession::set_data<std::string>(const std::string&, const std::string&) -> void;
    template auto GameSession::set_data<bool>(const std::string&, const bool&) -> void;
    
    template auto GameSession::get_data<int>(const std::string&) const -> std::optional<int>;
    template auto GameSession::get_data<float>(const std::string&) const -> std::optional<float>;
    template auto GameSession::get_data<std::string>(const std::string&) const -> std::optional<std::string>;
    template auto GameSession::get_data<bool>(const std::string&) const -> std::optional<bool>;
    
    auto GameSession::connection() const -> std::shared_ptr<GameConnection>
    {
        return current_connection();
    }
    
    auto GameSession::location() const -> Location
    {
        return current_location();
    }}