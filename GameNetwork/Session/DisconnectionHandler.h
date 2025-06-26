#pragma once

#include "../GameNetworkConstants.h"
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <any>
#include <tuple>

namespace GameNetwork
{
    class GameSession;
    class GameSessionManager;
    class GameConnection;
    
    struct DisconnectionInfo
    {
        std::string session_id;
        std::string account_id;
        uint64_t entity_id;
        Location last_location;
        uint32_t channel_id;
        std::chrono::steady_clock::time_point disconnect_time;
        DisconnectReason reason;
        
        // 세션 상태 백업
        std::unordered_map<std::string, std::any> session_data;
    };
    
    struct ReconnectionConfig
    {
        std::chrono::seconds grace_period = std::chrono::seconds(30);      // 재접속 유예 시간
        std::chrono::seconds max_grace_period = std::chrono::seconds(300); // 최대 유예 시간
        bool save_session_data = true;                                     // 세션 데이터 저장 여부
        bool restore_location = true;                                      // 위치 복원 여부
        size_t max_pending_sessions = 1000;                               // 최대 대기 세션 수
    };
    
    class DisconnectionHandler
    {
    public:
        using ReconnectionCallback = std::function<void(std::shared_ptr<GameSession>)>;
        using ExpirationCallback = std::function<void(const DisconnectionInfo&)>;
        
        DisconnectionHandler();
        ~DisconnectionHandler();
        
        // 초기화
        auto set_session_manager(std::shared_ptr<GameSessionManager> manager) -> void;
        auto set_config(const ReconnectionConfig& config) -> void;
        
        // 연결 끊김 처리
        auto handle_disconnection(std::shared_ptr<GameSession> session, 
                                DisconnectReason reason) -> void;
        
        // 재접속 처리
        auto handle_reconnection(const std::string& session_id, 
                               std::shared_ptr<GameConnection> connection) 
            -> std::tuple<bool, std::optional<std::string>>;
        
        // 재접속 가능 확인
        auto can_reconnect(const std::string& session_id) const -> bool;
        auto get_remaining_time(const std::string& session_id) const 
            -> std::optional<std::chrono::seconds>;
        
        // 콜백 등록
        auto register_reconnection_callback(ReconnectionCallback callback) -> void;
        auto register_expiration_callback(ExpirationCallback callback) -> void;
        
        // 통계
        struct Statistics
        {
            uint64_t total_disconnections;
            uint64_t successful_reconnections;
            uint64_t expired_sessions;
            uint64_t current_pending_sessions;
            double average_reconnection_time_ms;
        };
        
        auto get_statistics() const -> Statistics;
        
        // 유지보수
        auto cleanup_expired_sessions() -> size_t;
        auto force_expire_session(const std::string& session_id) -> bool;
        
    private:
        auto cleanup_thread() -> void;
        auto save_session_state(std::shared_ptr<GameSession> session) -> DisconnectionInfo;
        auto restore_session_state(std::shared_ptr<GameSession> session, 
                                 const DisconnectionInfo& info) -> void;
        
    private:
        std::weak_ptr<GameSessionManager> session_manager_;
        ReconnectionConfig config_;
        
        mutable std::mutex pending_mutex_;
        std::unordered_map<std::string, DisconnectionInfo> pending_sessions_;
        
        std::vector<ReconnectionCallback> reconnection_callbacks_;
        std::vector<ExpirationCallback> expiration_callbacks_;
        
        // 통계
        std::atomic<uint64_t> total_disconnections_{0};
        std::atomic<uint64_t> successful_reconnections_{0};
        std::atomic<uint64_t> expired_sessions_{0};
        
        // 정리 스레드
        std::atomic<bool> running_{false};
        std::thread cleanup_thread_;
        std::chrono::seconds cleanup_interval_{5};
    };
    
} // namespace GameNetwork
