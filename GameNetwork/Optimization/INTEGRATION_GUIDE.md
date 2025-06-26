# Performance Optimization Integration Guide

## 1. 통합 방법

### 1.1 GameNetwork 모듈 통합

#### Zero Copy 패킷 처리 적용
```cpp
// 기존 코드
auto GameNetworkServer::handle_packet(const std::vector<uint8_t>& data)
{
    auto packet = deserialize_packet(data); // 복사 발생
    process_packet(packet);
}

// 최적화된 코드
auto GameNetworkServer::handle_packet_optimized()
{
    // Zero-copy buffer 사용
    AsioZeroCopyBuffer& buffer = connection->get_buffer();
    
    // 직접 읽기 (복사 없음)
    if (auto packet_view = buffer.read_packet())
    {
        processor_->process_raw_packet(*packet_view);
    }
}
```

#### Lock-Free 메시지 큐 적용
```cpp
// GameNetworkServer.cpp 수정
class GameNetworkServer::Impl
{
private:
    // 기존 메시지 큐 대체
    std::shared_ptr<OptimizedPacketProcessor> packet_processor_;
    
public:
    auto initialize() -> void
    {
        packet_processor_ = std::make_shared<OptimizedPacketProcessor>(thread_pool_);
        
        // 패킷 핸들러 등록
        packet_processor_->register_handler(PacketType::MoveTo,
            [this](auto packet) { handle_movement(std::move(packet)); });
            
        // 워커 스레드 시작
        packet_processor_->start(8); // 8 worker threads
    }
};
```

### 1.2 GameDatabase 모듈 통합

#### Lock-Free Connection Pool 적용
```cpp
// DBConnectionPool.h를 OptimizedDBConnectionPool로 교체
class GameDatabase::Manager
{
private:
    // std::shared_ptr<DBConnectionPool> pool_; // 기존
    std::shared_ptr<OptimizedDBConnectionPool> pool_; // 최적화
    
public:
    auto execute_query_async(const std::string& query)
    {
        return pool_->execute_async<ResultSet>(
            [query](auto conn) { return conn->execute(query); });
    }
};
```

## 2. 성능 측정 방법

### 2.1 지연시간 측정
```cpp
class LatencyMonitor
{
public:
    auto measure_packet_latency() -> void
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 패킷 처리
        process_packet();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        update_statistics(latency.count());
    }
};
```

### 2.2 처리량 측정
```cpp
class ThroughputMonitor
{
private:
    std::atomic<uint64_t> packets_processed_{0};
    std::chrono::steady_clock::time_point start_time_;
    
public:
    auto get_throughput() -> double
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        auto seconds = std::chrono::duration<double>(elapsed).count();
        return packets_processed_.load() / seconds;
    }
};
```

## 3. 구성 권장사항

### 3.1 버퍼 크기 설정
```cpp
// 네트워크 버퍼 크기 (연결당)
constexpr size_t NETWORK_BUFFER_SIZE = 1 << 20;  // 1MB

// 메시지 큐 크기
constexpr size_t MESSAGE_QUEUE_SIZE = 1 << 16;   // 65,536 messages

// 데이터베이스 연결 풀 크기
constexpr size_t DB_POOL_SIZE = 32;              // 32 connections
```

### 3.2 스레드 구성
```cpp
// CPU 코어 수에 따른 워커 스레드 수
auto worker_count = std::thread::hardware_concurrency();

// I/O 스레드 (Boost.Asio)
auto io_threads = worker_count / 2;

// 패킷 처리 스레드
auto packet_threads = worker_count;

// 데이터베이스 스레드
auto db_threads = worker_count / 4;
```

## 4. 모니터링 및 디버깅

### 4.1 성능 메트릭 수집
```cpp
struct PerformanceMetrics
{
    // 지연시간 통계
    double avg_latency_ms;
    double p99_latency_ms;
    double max_latency_ms;
    
    // 처리량 통계
    double packets_per_second;
    double bytes_per_second;
    
    // 리소스 사용량
    double cpu_usage_percent;
    double memory_usage_mb;
    
    // 에러율
    double packet_drop_rate;
    double connection_error_rate;
};
```

### 4.2 프로파일링 포인트
```cpp
// 주요 병목 지점 프로파일링
PROFILE_SCOPE("PacketDeserialization");
PROFILE_SCOPE("DatabaseQuery");
PROFILE_SCOPE("NetworkSend");
```

## 5. 마이그레이션 체크리스트

- [ ] 기존 패킷 직렬화 코드를 Zero Copy 버전으로 교체
- [ ] 메시지 큐를 Lock-Free 버전으로 교체
- [ ] 데이터베이스 연결 풀을 최적화 버전으로 교체
- [ ] Boost.Asio 핸들러에 커스텀 할당자 적용
- [ ] 성능 모니터링 코드 추가
- [ ] 부하 테스트 실행
- [ ] 프로덕션 환경 단계적 배포

## 6. 주의사항

1. **메모리 정렬**: 캐시 라인 정렬이 중요하므로 반드시 확인
2. **ABA 문제**: Lock-Free 구조에서 포인터 재사용 시 주의
3. **메모리 순서**: 적절한 memory_order 플래그 사용
4. **호환성**: 기존 인터페이스 유지하며 점진적 마이그레이션
