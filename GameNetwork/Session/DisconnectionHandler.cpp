#include "DisconnectionHandler.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "../../Samples/Location.h"
#include <Logger.h>
#include <chrono>
#include <algorithm>

namespace GameNetwork
{
    DisconnectionHandler::DisconnectionHandler()
    {
        running_ = true;
        cleanup_thread_ = std::thread(&DisconnectionHandler::cleanup_thread, this);
    }
    
    DisconnectionHandler::~DisconnectionHandler()
    {
        running_ = false;
        if (cleanup_thread_.joinable())
        {
            cleanup_thread_.join();
        }
    }
    
    auto DisconnectionHandler::set_session_manager(std::shared_ptr<GameSessionManager> manager) -> void
    {
        session_manager_ = manager;
    }
    
    auto DisconnectionHandler::set_config(const ReconnectionConfig& config) -> void
    {
        config_ = config;
    }
    
    auto DisconnectionHandler::handle_disconnection(std::shared_ptr<GameSession> session, 
                                                   DisconnectReason reason) -> void
    {
        if (!session)
        {
            return;
        }
        
        auto session_id = session->session_id();
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Handling disconnection for session: " + session_id);
        
        // Session state save
        auto info = save_session_state(session);
        info.reason = reason;
        
        // 대기 목록에 추가
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            
            // 최대 대기 세션 수 확인
            if (pending_sessions_.size() >= config_.max_pending_sessions)
            {
                // 가장 오래된 세션 제거
                auto oldest = std::min_element(pending_sessions_.begin(), pending_sessions_.end(),
                    [](const auto& a, const auto& b) {
                        return a.second.disconnect_time < b.second.disconnect_time;
                    });
                
                if (oldest != pending_sessions_.end())
                {
                    for (const auto& callback : expiration_callbacks_)
                    {
                        callback(oldest->second);
                    }
                    pending_sessions_.erase(oldest);
                    expired_sessions_++;
                }
            }
            
            pending_sessions_[session_id] = std::move(info);
        }
        
        total_disconnections_++;
        
        // 세션 상태를 Disconnected로 변경
        session->set_state(SessionConnectionState::Disconnected);
    }
    
    auto DisconnectionHandler::handle_reconnection(const std::string& session_id, 
                                                  std::shared_ptr<GameConnection> connection) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::unique_lock<std::mutex> lock(pending_mutex_);
        
        auto it = pending_sessions_.find(session_id);
        if (it == pending_sessions_.end())
        {
            return {false, "Session not found in pending list"};
        }
        
        auto& info = it->second;
        
        // 유예 시간 확인
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info.disconnect_time);
        
        if (elapsed > config_.grace_period)
        {
            // 유예 시간 초과
            pending_sessions_.erase(it);
            expired_sessions_++;
            return {false, "Grace period expired"};
        }
        
        // 세션 매니저에서 세션 찾기
        auto manager = session_manager_.lock();
        if (!manager)
        {
            return {false, "Session manager not available"};
        }
        
        auto session = manager->find_session(session_id);
        if (!session)
        {
            // Session not found, create new one
            session = std::make_shared<GameSession>(session_id, info.account_id);
            manager->add_session(session);
        }
        
        // 연결 바인딩
        session->bind_connection(connection);
        
        // 세션 상태 복원
        restore_session_state(session, info);
        
        // 대기 목록에서 제거
        pending_sessions_.erase(it);
        lock.unlock();
        
        // 재접속 시간 기록
        auto reconnection_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.disconnect_time);
        
        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Session reconnected: " + session_id + " after " + 
            std::to_string(reconnection_time.count()) + "ms");
        
        successful_reconnections_++;
        
        // Track reconnection time
        reconnection_times_.push_back(reconnection_time);
        
        // Keep only recent reconnection times (last 100)
        if (reconnection_times_.size() > 100)
        {
            reconnection_times_.erase(reconnection_times_.begin());
        }
        
        // 콜백 호출
        for (const auto& callback : reconnection_callbacks_)
        {
            callback(session);
        }
        
        return {true, std::nullopt};
    }
    
    auto DisconnectionHandler::can_reconnect(const std::string& session_id) const -> bool
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        auto it = pending_sessions_.find(session_id);
        if (it == pending_sessions_.end())
        {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.disconnect_time);
        
        return elapsed <= config_.grace_period;
    }
    
    auto DisconnectionHandler::get_remaining_time(const std::string& session_id) const 
        -> std::optional<std::chrono::seconds>
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        auto it = pending_sessions_.find(session_id);
        if (it == pending_sessions_.end())
        {
            return std::nullopt;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.disconnect_time);
        
        if (elapsed >= config_.grace_period)
        {
            return std::chrono::seconds(0);
        }
        
        return config_.grace_period - elapsed;
    }
    
    auto DisconnectionHandler::register_reconnection_callback(ReconnectionCallback callback) -> void
    {
        reconnection_callbacks_.push_back(callback);
    }
    
    auto DisconnectionHandler::register_expiration_callback(ExpirationCallback callback) -> void
    {
        expiration_callbacks_.push_back(callback);
    }
    
    auto DisconnectionHandler::get_statistics() const -> Statistics
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        Statistics stats;
        stats.total_disconnections = total_disconnections_.load();
        stats.successful_reconnections = successful_reconnections_.load();
        stats.expired_sessions = expired_sessions_.load();
        stats.current_pending_sessions = pending_sessions_.size();
        
        // 평균 재접속 시간 계산 (간단한 구현)
        if (reconnection_times_.empty())
        {
            stats.average_reconnection_time_ms = 0.0;
        }
        else
        {
            double total_time = 0.0;
            for (const auto& time : reconnection_times_)
            {
                total_time += time.count();
            }
            stats.average_reconnection_time_ms = total_time / reconnection_times_.size();
        }
        
        return stats;
    }
    
    auto DisconnectionHandler::cleanup_expired_sessions() -> size_t
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        size_t removed_count = 0;
        
        for (auto it = pending_sessions_.begin(); it != pending_sessions_.end();)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.disconnect_time);
            
            if (elapsed > config_.grace_period)
            {
                // 만료 콜백 호출
                for (const auto& callback : expiration_callbacks_)
                {
                    callback(it->second);
                }
                
                it = pending_sessions_.erase(it);
                expired_sessions_++;
                removed_count++;
            }
            else
            {
                ++it;
            }
        }
        
        return removed_count;
    }
    
    auto DisconnectionHandler::force_expire_session(const std::string& session_id) -> bool
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        auto it = pending_sessions_.find(session_id);
        if (it == pending_sessions_.end())
        {
            return false;
        }
        
        // 만료 콜백 호출
        for (const auto& callback : expiration_callbacks_)
        {
            callback(it->second);
        }
        
        pending_sessions_.erase(it);
        expired_sessions_++;
        
        return true;
    }
    
    auto DisconnectionHandler::cleanup_thread() -> void
    {
        while (running_.load())
        {
            std::this_thread::sleep_for(cleanup_interval_);
            
            auto removed = cleanup_expired_sessions();
            
            if (removed > 0)
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                    "Cleaned up " + std::to_string(removed) + " expired sessions");
            }
        }
    }
    
    auto DisconnectionHandler::save_session_state(std::shared_ptr<GameSession> session) 
        -> DisconnectionInfo
    {
        DisconnectionInfo info;
        info.session_id = session->session_id();
        info.account_id = session->account_id();
        info.entity_id = session->get_entity_id();
        info.last_location = 0; // Stubbed out location temporarily
        info.channel_id = session->current_channel_id();
        info.disconnect_time = std::chrono::steady_clock::now();
        
        // 세션의 커스텀 데이터 저장
        if (config_.save_session_data)
        {
            // Save custom session data
            info.session_data = boost::json::object();
            info.session_data["entity_id"] = info.entity_id;
            info.session_data["location"] = info.last_location;  // Now just an int
            info.session_data["channel_id"] = info.channel_id;
            info.session_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                info.disconnect_time.time_since_epoch()).count();
            
            // Add any custom data from the session
            auto custom_data = session->get_custom_data();
            if (!custom_data.empty())
            {
                info.session_data["custom"] = custom_data;
            }
        }
        
        return info;
    }
    
    auto DisconnectionHandler::restore_session_state(std::shared_ptr<GameSession> session, 
                                                    const DisconnectionInfo& info) -> void
    {
        // 세션 상태 복원
        session->set_state(SessionConnectionState::Connected);
        
        // 위치 복원
        // 위치 복원 (temporarily commented out)
        // if (config_.restore_location)
        // {
        //     session->teleport_to(info.last_location);
        // }
        
        // 채널 복원
        if (info.channel_id > 0)
        {
            session->enter_channel(info.channel_id);
        }
        
        // 커스텀 데이터 복원
        if (config_.save_session_data && info.session_data.contains("custom"))
        {
            try
            {
                auto custom_obj = info.session_data.at("custom").as_object();
                for (const auto& pair : custom_obj)
                {
                    session->set_custom_data(std::string(pair.key()), pair.value());
                }
            }
            catch (const std::exception& e)
            {
                Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                    "Failed to restore custom session data: " + std::string(e.what()));
            }
        }
        //         session->set_data(key, value);
        //     }
        // }
    }
    
} // namespace GameNetwork
