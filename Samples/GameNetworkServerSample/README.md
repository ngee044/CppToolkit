# GameNetwork Samples

MMORPG 게임 네트워크 개발을 위한 샘플 프로그램들입니다.

## GameNetworkServerSample

MMORPG 게임 서버 샘플입니다. GameNetwork 모듈의 서버 기능들을 시연합니다.

### 주요 기능
- 게임 서버 시작 및 관리
- 플레이어 세션 관리
- 패킷 핸들러 등록 및 처리
- Redis를 통한 세션 영속성
- 실시간 서버 통계 모니터링

### 사용법
```bash
GameNetworkServerSample [options]

Options:
  --port <port>              서버 포트 (기본값: 9090)
  --max-players <count>      최대 플레이어 수 (기본값: 1000)
  --max-channels <count>     최대 채널 수 (기본값: 10)
  --server-id <id>           서버 ID
  --server-name <name>       서버 이름
  --redis-host <host>        Redis 호스트 (기본값: localhost)
  --redis-port <port>        Redis 포트 (기본값: 6379)
  --redis-password <pass>    Redis 비밀번호
  --log-level <0-7>          로그 레벨
  --help                     도움말 표시
```

### 예제
```bash
# 기본 설정으로 서버 시작
GameNetworkServerSample

# 커스텀 설정으로 서버 시작
GameNetworkServerSample --port 8080 --max-players 5000 --server-id game01

# Redis 연결 설정
GameNetworkServerSample --redis-host 192.168.1.100 --redis-password mypassword
```

## GameNetworkClientSample

MMORPG 게임 클라이언트 샘플입니다. 서버 연결, 인증, 게임플레이를 시뮬레이션합니다.

### 주요 기능
- 서버 연결 및 인증
- 자동 플레이 모드 (이동, 채팅)
- 네트워크 품질 모니터링
- 지연 보상 시뮬레이션

### 사용법
```bash
GameNetworkClientSample [options]

Options:
  --server <address>         서버 주소 (기본값: localhost)
  --port <port>              서버 포트 (기본값: 9090)
  --account <id>             계정 ID
  --token <token>            세션 토큰
  --auto-play                자동 플레이 모드 활성화
  --move-interval <ms>       이동 간격 (기본값: 5000ms)
  --chat-interval <ms>       채팅 간격 (기본값: 30000ms)
  --log-level <0-7>          로그 레벨
  --help                     도움말 표시
```

### 예제
```bash
# 기본 설정으로 연결
GameNetworkClientSample --account player123

# 자동 플레이 모드로 실행
GameNetworkClientSample --account player123 --auto-play

# 원격 서버에 연결
GameNetworkClientSample --server game.example.com --port 8080 --account player123
```

## 테스트 시나리오

### 1. 기본 연결 테스트
```bash
# 터미널 1: 서버 시작
GameNetworkServerSample

# 터미널 2: 클라이언트 연결
GameNetworkClientSample --account test_player --auto-play
```

### 2. 다중 클라이언트 테스트
```bash
# 서버 시작
GameNetworkServerSample --max-players 100

# 여러 클라이언트 연결 (각각 다른 터미널에서)
GameNetworkClientSample --account player1 --auto-play
GameNetworkClientSample --account player2 --auto-play
GameNetworkClientSample --account player3 --auto-play
```

### 3. 네트워크 품질 시뮬레이션
네트워크 품질이 변할 때 시스템이 어떻게 적응하는지 관찰할 수 있습니다.
- 서버와 클라이언트 로그에서 네트워크 통계 확인
- RTT, 패킷 손실률, 대역폭 사용량 모니터링
- 네트워크 품질에 따른 업데이트 빈도 조정 확인

## 로그 파일
- 서버: `GameNetworkServerSample.log`
- 클라이언트: `GameNetworkClientSample.log`

로그 레벨:
- 0: None
- 1: Error
- 2: Warning
- 4: Information
- 7: All (Error | Warning | Information)
- 패킷 레벨 로깅은 디버그 빌드에서만 활성화됩니다.
