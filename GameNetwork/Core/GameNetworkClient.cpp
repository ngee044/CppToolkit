#include "GameNetworkClient.h"
#include "../Packet/GamePacket.h"
#include "../Session/GameConnection.h"
#include "../Session/GameSession.h"
#include "../../Utilities/Logger.h"
#include "../../ThreadPool/ThreadPool.h"
#include "../../ThreadPool/Job.h"
#include <thread>

using namespace Utilities;

namespace GameNetwork {

GameNetworkClient::GameNetworkClient(const ClientConfig& config)
    : config_(config)
    , network_client_(nullptr)
    , thread_pool_(nullptr)
    , current_session_(nullptr)
    , is_running_(false)
    , connection_state_(ConnectionState::Disconnected)
{
}

GameNetworkClient::~GameNetworkClient() {
    disconnect();
}

bool GameNetworkClient::initialize() {
    Logger::handle().write(LogTypes::Information, "Initializing GameNetworkClient...");
    
    // ThreadPool 생성
    thread_pool_ = std::make_shared<Thread::ThreadPool>("GameNetworkClient");
    
    if (!thread_pool_) {
        Logger::handle().write(LogTypes::Error, "Failed to create ThreadPool");
        return false;
    }
    
    auto start_result = thread_pool_->start();
    if (!std::get<0>(start_result)) {
        std::string error = std::get<1>(start_result).value_or("Unknown error");
        Logger::handle().write(LogTypes::Error, "Failed to start ThreadPool: " + error);
        return false;
    }
    
    // NetworkClient 생성 (실제 API에 맞게 수정)
    network_client_ = std::make_unique<Network::NetworkClient>(
        config_.client_id,
        3, // high_priority_count
        3, // normal_priority_count
        3  // low_priority_count
    );
    
    if (!network_client_) {
        Logger::handle().write(LogTypes::Error, "Failed to create NetworkClient");
        return false;
    }
    
    // 콜백 설정
    network_client_->received_connection_callback([this](const bool& connected, const bool& by_server) {
        if (connected) {
            onConnected(nullptr); // NetworkSession은 내부에서 관리됨
        } else {
            onDisconnected(nullptr);
        }
        return std::make_tuple(true, std::nullopt);
    });
    
    network_client_->received_binary_callback([this](const std::string& sender_id, const std::vector<uint8_t>& data) {
        onDataReceived(nullptr, data); // NetworkSession은 내부에서 관리됨
        return std::make_tuple(true, std::nullopt);
    });
    
    is_running_ = true;
    Logger::handle().write(LogTypes::Information, "GameNetworkClient initialized successfully");
    return true;
}

void GameNetworkClient::shutdown() {
    Logger::handle().write(LogTypes::Information, "Shutting down GameNetworkClient...");
    
    disconnect();
    
    if (network_client_) {
        network_client_->stop();
        network_client_.reset();
    }
    
    if (thread_pool_) {
        thread_pool_->stop();
        thread_pool_.reset();
    }
    
    is_running_ = false;
    Logger::handle().write(LogTypes::Information, "GameNetworkClient shutdown complete");
}

bool GameNetworkClient::connect() {
    if (!network_client_) {
        Logger::handle().write(LogTypes::Error, "NetworkClient is not initialized");
        return false;
    }
    
    if (connection_state_ == ConnectionState::Connected || 
        connection_state_ == ConnectionState::Connecting) {
        Logger::handle().write(LogTypes::Error, "Already connected or connecting");
        return true;
    }
    
    connection_state_ = ConnectionState::Connecting;
    Logger::handle().write(LogTypes::Information, "Connecting to server " + config_.server_ip + ":" + std::to_string(config_.server_port));
    
    bool result = network_client_->start(config_.server_ip, config_.server_port, config_.max_buffer_size);
    if (!result) {
        connection_state_ = ConnectionState::Disconnected;
        Logger::handle().write(LogTypes::Error, "Failed to connect to server");
    }
    
    return result;
}

void GameNetworkClient::disconnect() {
    if (connection_state_ == ConnectionState::Disconnected) {
        return;
    }
    
    Logger::handle().write(LogTypes::Information, "Disconnecting from server...");
    connection_state_ = ConnectionState::Disconnecting;
    
    if (network_client_) {
        network_client_->stop();
    }
    
    connection_state_ = ConnectionState::Disconnected;
    Logger::handle().write(LogTypes::Information, "Disconnected from server");
}

bool GameNetworkClient::sendPacket(const GamePacket& packet) {
    if (!network_client_ || connection_state_ != ConnectionState::Connected) {
        Logger::handle().write(LogTypes::Error, "Cannot send packet: not connected");
        return false;
    }
    
    try {
        auto serialized_data = packet.serialize();
        auto result = network_client_->send_binary(serialized_data, "game_packet");
        return std::get<0>(result);
    } catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Failed to send packet: " + std::string(e.what()));
        return false;
    }
}

void GameNetworkClient::onConnected(std::shared_ptr<Network::NetworkSession> session) {
    if (!session) {
        Logger::handle().write(LogTypes::Error, "Connected callback received null session, but connection is established");
    }
    
    connection_state_ = ConnectionState::Connected;
    current_session_ = session;
    
    Logger::handle().write(LogTypes::Information, "Connected to server successfully");
    
    // 연결 완료 이벤트 처리를 스레드풀에서 비동기로 실행
    if (thread_pool_) {
        auto job = std::make_shared<Thread::Job>(
            Thread::JobPriorities::Normal,
            [this]() -> std::tuple<bool, std::optional<std::string>> {
                // 연결 완료 후 초기화 작업
                Logger::handle().write(LogTypes::Information, "Connection established, performing post-connection setup");
                return {true, std::nullopt};
            },
            "PostConnectionSetup"
        );
        thread_pool_->push(job);
    }
}

void GameNetworkClient::onDisconnected(std::shared_ptr<Network::NetworkSession> session) {
    Logger::handle().write(LogTypes::Information, "Disconnected from server");
    
    connection_state_ = ConnectionState::Disconnected;
    current_session_.reset();
    
    // 재연결 로직 등을 스레드풀에서 비동기로 처리
    if (thread_pool_ && config_.auto_reconnect) {
        auto job = std::make_shared<Thread::Job>(
            Thread::JobPriorities::Normal,
            [this]() -> std::tuple<bool, std::optional<std::string>> {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (is_running_ && connection_state_ == ConnectionState::Disconnected) {
                    Logger::handle().write(LogTypes::Information, "Attempting auto-reconnect...");
                    connect();
                }
                return {true, std::nullopt};
            },
            "AutoReconnect"
        );
        thread_pool_->push(job);
    }
}

void GameNetworkClient::onDataReceived(std::shared_ptr<Network::NetworkSession> session, 
                                     const std::vector<uint8_t>& data) {
    if (!thread_pool_) {
        Logger::handle().write(LogTypes::Error, "ThreadPool is null, cannot process received data");
        return;
    }
    
    // 패킷 처리를 스레드풀에서 비동기로 실행
    auto job = std::make_shared<Thread::Job>(
        Thread::JobPriorities::High,
        [this, data]() -> std::tuple<bool, std::optional<std::string>> {
            processReceivedData(data);
            return {true, std::nullopt};
        },
        "PacketProcessing"
    );
    thread_pool_->push(job);
}

void GameNetworkClient::processReceivedData(const std::vector<uint8_t>& data) {
    try {
        // 수신된 데이터 처리를 위한 기본 로깅
        Logger::handle().write(LogTypes::Debug, 
            "Received " + std::to_string(data.size()) + " bytes");
        
        // 패킷 프로세서와 메시지 디스패처를 사용하여 처리
        if (packet_processor_) {
            auto packet = packet_processor_->deserialize_binary(data);
            if (packet && message_dispatcher_) {
                message_dispatcher_->dispatch(std::move(*packet));
                stats_.packets_received++;
                stats_.bytes_received += data.size();
            }
        }
        
    } catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, 
            "Failed to process received data: " + std::string(e.what()));
    }
}

void GameNetworkClient::handlePacket(const GamePacket& packet) {
    // 기본 패킷 처리 로직
    Logger::handle().write(LogTypes::Debug, "Received packet");
    
    // 상속받은 클래스에서 구체적인 패킷 처리를 구현할 수 있도록
    // 가상 함수로 만들거나 콜백을 사용할 수 있음
}

bool GameNetworkClient::isConnected() const {
    return connection_state_ == ConnectionState::Connected;
}

ConnectionState GameNetworkClient::getConnectionState() const {
    return connection_state_;
}

} // namespace GameNetwork