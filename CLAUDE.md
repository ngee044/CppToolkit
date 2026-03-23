# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Full clean build (recommended)
./build.sh

# Manual build
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build -j

# Run samples (executables output to build/bin/)
./build/bin/NetworkServerSample
./build/bin/ThreadSample
```

Disable specific modules at configure time: `-DBUILD_NETWORK_LIB=OFF`, `-DBUILD_REDIS_LIB=OFF`, `-DBUILD_RABBIT_MQ_LIB=OFF`, `-DBUILD_KAFKA_LIB=OFF`, `-DBUILD_SAMPLES=OFF`.

**Prerequisites:** vcpkg at `../../vcpkg` (relative to source root), Ninja, CMake 3.18+.

## Architecture

CppToolkit is a modular C++23 library. All modules depend on Utilities (foundation) and ThreadPool:

```
Utilities  (Logger, Converter, File/Folder, Compressor, ArgumentParser, FolderWatcher)
    └── ThreadPool  (priority-based Job scheduling with worker threads)
            ├── Network    (async TCP server/client via Boost.Asio + strand pattern)
            ├── Database   (PostgreSQL via libpq, DatabaseInterface abstraction)
            ├── Redis      (redis-plus-plus wrapper with TLS/connection pooling)
            ├── RabbitMQ   (AMQP producer/consumer with multi-channel support)
            └── Kafka      (modern-cpp-kafka producer/consumer/admin)
```

### Key Patterns

**Error handling** — All public APIs return `std::tuple<bool, std::optional<std::string>>` (success bool + error message). No exceptions in async/network code paths. Database queries returning data use `std::optional<vector<vector<variant<...>>>>`.

**Threading model** — ThreadPool manages priority-based workers (Top/High/Normal/Low). Job callbacks follow the same tuple return pattern. Network module uses Boost.Asio strands to serialize socket operations per session.

**Async I/O** — Network uses Boost.Asio io_context with strand-per-session. RabbitMQ and Kafka use promise/future for lifecycle + ThreadPool for background work. All modules support explicit start/stop with `wait_stop()` for graceful shutdown.

**Memory management** — `shared_ptr` for shared ownership, `weak_ptr` to break circular references (parent-child), `enable_shared_from_this` for self-referencing. Logger is the only singleton (`Logger::handle()` with `call_once`).

### Network Protocol

DataHandler implements a binary protocol: start code → length → data → end code. Supports modes: Binary, File, Message, Connection. Optional encryption (cryptopp) and compression (lz4) layers. Sessions have heartbeat/maintenance for connection health.

## Code Style

- **Format:** `.clang-format` — GNU-based, tabs (width 4), 170 column limit. Apply: `clang-format -i <file>`
- **Standard:** C++23 (`std::format`, `std::optional`, `std::variant`, `std::expected`)
- **Naming:** PascalCase classes/files, snake_case functions/variables, `member_` suffix for class members
- **Headers:** `#pragma once`, unsorted includes (`SortIncludes: false`), project headers in quotes, system in angle brackets
- **Namespaces:** `Utilities`, `Thread`, `Network` — all contents indented (NamespaceIndentation: All)

## Key Dependencies (vcpkg.json)

boost-asio, boost-json, boost-filesystem, boost-process, boost-algorithm, boost-dll, lz4, cryptopp, libpq, redis-plus-plus (cxx17/tls), librabbitmq, modern-cpp-kafka, efsw, curl, openssl, gtest

## Validation

정식 유닛 테스트 스위트가 없으므로, 프레임워크 코드 변경 시 **반드시 `docs/sample_test_cases.md`의 테스트 케이스를 기준으로 검증**한다.

### 검증 절차

1. `./build.sh`로 전체 빌드가 성공하는지 확인
2. 변경된 모듈에 해당하는 샘플의 테스트 케이스를 `docs/sample_test_cases.md`에서 찾아 실행
3. 각 테스트 케이스의 "확인 포인트"에 명시된 로그/동작이 기대대로 나오는지 확인

### 모듈별 샘플 매핑

| 변경 모듈 | 실행할 샘플 | 주요 테스트 케이스 |
|-----------|------------|-------------------|
| Utilities (Logger, Converter, File, Folder, ArgumentParser) | 모든 샘플 (공통 의존) | 공통 검증 항목 |
| ThreadPool, Job, ThreadWorker | ThreadSample | TC-1.1 ~ TC-1.4 |
| Network (Server/Client/Session/DataHandler) | NetworkServerSample + NetworkClientSample | TC-2.1 ~ TC-2.8 |
| Database (PostgresDB) | DatabaseSample | TC-3.1 ~ TC-3.6 |
| FolderWatcher | FolderWatcherSample | TC-4.1 ~ TC-4.3 |
| Folder (compression/decompression) | FolderCompressionSample | TC-5.1 ~ TC-5.6 |
| Kafka | KafkaSample | TC-6.1 ~ TC-6.4 |

새로운 기능을 추가할 때는 관련 샘플에 테스트 시나리오를 추가하고, `docs/sample_test_cases.md`에 테스트 케이스를 함께 업데이트한다.
