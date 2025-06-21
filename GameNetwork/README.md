# GameNetwork Module

MMORPG에 최적화된 게임 네트워크 모듈입니다. 기존 Network 모듈을 기반으로 대규모 동시접속, 채널 시스템, 재접속 등 온라인 게임에 필요한 기능들을 구현합니다.

## 주요 특징

### 1. 계층적 아키텍처
- **Transport Layer**: 기존 Network 모듈 활용
- **Session Layer**: GameConnection을 통한 연결 관리
- **Logic Layer**: GameSession을 통한 게임 로직 처리

### 2. 핵심 기능
- **세션 관리**: 로그인부터 로그아웃까지 플레이어 상태 유지
- **재접속 지원**: 연결 끊김 시 일정 시간 동안 세션 유지
- **채널 시스템**: 다중 채널 지원 및 채널 간 이동
- **영역 관리**: Area of Interest (AOI) 기반 동기화
- **패킷 최적화**: 압축, 암호화, 배칭 지원

### 3. 보안 기능
- **패킷 검증**: 유효성 검사 및 치트 방지
- **Rate Limiting**: DDoS 방어를 위한 전송률 제한
- **암호화**: OpenSSL을 통한 패킷 암호화

## 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                    Game Client                          │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────┴────────────────────────────────┐
│                  GameNetworkServer                      │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐   │
│  │   Message   │  │    World     │  │    Packet     │   │
│  │ Dispatcher  │  │ Synchronizer │  │  Processor    │   │
│  └──────┬──────┘  └──────┬───────┘  └───────┬───────┘   │
│         │                │                  │           │
│  ┌──────┴────────────────┴──────────────────┴────────┐  │
│  │              GameSessionManager                   │  │
│  └───────────────────────────────────────────────────┘  │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐   │
│  │ GameSession │  │GameConnection│  │NetworkSession │   │
│  └─────────────┘  └──────────────┘  └───────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## 사용 예제

### 서버 시작
```cpp
#include "GameNetwork/Core/GameNetworkServer.h"

// 서버 설정
GameNetwork::ServerConfig config;
config.server_id = "game_server_01";
config.server_name = "Game Server 1";
config.port = 8080;
config.max_players = 1000;
config.max_channels = 10;
config.socket_buffer_size = 65536;

// 서버 생성 및 시작
auto server = std::make_shared<GameNetwork::GameNetworkServer>(config);
auto [success, error] = server->start();

if (!success)
{
    std::cerr << "Failed to start server: " << *error << std::endl;
    return -1;
}

// 세션 이벤트 핸들러 등록
server->session_manager()->on_session_connected(
    [](std::shared_ptr<GameNetwork::GameSession> session)
    {
        std::cout << "Player connected: " << session->account_id() << std::endl;
    });
```

### 패킷 핸들러 등록
```cpp
// 메시지 디스패처 가져오기
auto dispatcher = server->message_dispatcher();

// 이동 패킷 핸들러 등록
dispatcher->register_handler(
    GameNetwork::PacketType::MoveTo,
    [](std::shared_ptr<GameNetwork::GameSession> session, 
       const GameNetwork::GamePacket& packet) 
    -> std::tuple<bool, std::optional<std::string>>
    {
        // 이동 처리 로직
        auto& move_packet = static_cast<const MoveToPacket&>(packet);
        session->move_to(move_packet.destination());
        
        // 주변 플레이어에게 브로드캐스트
        server->world_synchronizer()->broadcast_to_area(
            session->current_location(), 
            100.0f,  // 반경 100m
            move_packet,
            session  // 자신 제외
        );
        
        return {true, std::nullopt};
    });
```

## 빌드

### 요구사항
- C++17 이상
- CMake 3.18 이상
- vcpkg (패키지 관리)
- Boost
- OpenSSL (암호화 옵션)
- ZLIB (압축 옵션)

### 빌드 방법
```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## 성능 최적화

- **멀티스레드**: ThreadPool을 활용한 비동기 처리
- **영역 기반 동기화**: 관심 영역 내 엔티티만 동기화
- **패킷 배칭**: 여러 패킷을 하나로 묶어 전송
- **LOD 시스템**: 거리에 따른 상세도 조절

## 향후 계획

- [ ] P2P 네트워킹 지원
- [ ] 더 정교한 치트 방지 시스템
- [ ] 네트워크 예측 및 보간
- [ ] 클라우드 기반 서버 마이그레이션
