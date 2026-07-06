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

**Error handling** — The standard is `std::expected` uniformly: `std::expected<void, std::string>` for pure success/failure APIs (`{}` on success, `std::unexpected("reason")` on failure) and `std::expected<T, std::string>` for data-returning-and-fallible APIs (value on success, `std::unexpected` on failure). The legacy `std::tuple<std::optional<T>, std::optional<std::string>>` pattern has been retired from library code (it may still appear in sample-local helpers). No exceptions in async/network code paths. **Conventions still differ per module** (some return `void`, `bool`, `std::optional<T>`, or a value directly) — see `docs/module_usage_guide.md` §4 for the per-API return-type table before calling.

**Threading model** — ThreadPool manages priority-based workers (`Top`/`High`/`Normal`/`Low`/`LongTerm`). **Job callbacks return `std::expected<void, std::string>`** (`return {};` / `std::unexpected`). Network module uses Boost.Asio strands to serialize socket operations per session.

**Async I/O** — Network uses Boost.Asio io_context with strand-per-session. RabbitMQ and Kafka use promise/future for lifecycle + ThreadPool for background work. Lifecycle is explicit **설정 → start → 사용 → stop → reset**. Shutdown API differs: `NetworkServer`/`RabbitMQ`/`Kafka` expose `wait_stop()`; **`ThreadPool` has NO `wait_stop()` — its `stop()` blocks until workers join** (`stop(true)` drops queued jobs).

**Memory management** — `shared_ptr` for shared ownership, `weak_ptr` to break circular references (parent-child), `enable_shared_from_this` for self-referencing. Logger is the only singleton (`Logger::handle()` with `call_once`).

### Network Protocol

DataHandler implements a binary protocol: start code → length → data → end code. Supports modes: Binary, File, Message, Connection. Optional encryption (cryptopp) and compression (lz4) layers. Sessions have heartbeat/maintenance for connection health.

## 핵심 규칙 (철칙) — 한 번에 컴파일되는 코드

> 이 툴킷으로 코드를 짤 때 거의 항상 여기서 처음 막힌다. 상세·모듈별 사용 예제·CMake 작성법은 **`docs/module_usage_guide.md`** (`CPPTK-USAGE-001`), 명명·스타일·관용구는 **`docs/code_convention.md`** (`CPPTK-CONV-001`)를 따른다.

1. **include는 평평하다(flat).** `#include "Logger.h"` — 경로 접두어(`"Utilities/Logger.h"`) 금지. 각 모듈이 소스 디렉터리를 `PUBLIC`으로 공개하기 때문.

2. **폴더명 ≠ 링크 타겟명 ≠ 네임스페이스.** `target_link_libraries`에는 타겟명을 쓴다:

   | 폴더 | 링크 타겟 | 네임스페이스 |
   |------|-----------|--------------|
   | `Utilities/` | `Utilities` | `Utilities` |
   | `ThreadPool/` | **`Thread`** ⚠️ | **`Thread`** ⚠️ |
   | `Network/` | `Network` | `Network` |
   | `Database/` | `Database` | `Database` |
   | `Redis/` | `Redis` | `Redis` |
   | `RabbitMQ/` | `RabbitMQ` | `RabbitMQ` |
   | `Kafka/` | `Kafka` | `Kafka` |

3. **vcpkg 패키지명 ≠ `find_package` 이름 ≠ 링크 타겟.** 새 외부 의존을 붙일 때만 해당(기존 모듈 소비 시엔 위 타겟만 링크하면 전파됨): `libpq`→`PostgreSQL`→`PostgreSQL::PostgreSQL`, `redis-plus-plus`→`redis++`→`redis++::redis++[_static]`, `librabbitmq`→`rabbitmq-c`→`rabbitmq::rabbitmq[-static]`, `modern-cpp-kafka`→`unofficial-modern-cpp-kafka`→`unofficial::modern-cpp-kafka::modern-cpp-kafka`, `lz4`→`lz4::lz4`, `efsw`→`efsw::efsw`, `cryptopp`→`cryptopp::cryptopp`.

4. **반환 타입은 모듈마다 다르다.** 넘겨짚지 말고 `module_usage_guide.md` §4 표로 확인. 대표 함정: `NetworkServer::start`→`std::expected` / `NetworkClient::start`→`bool`, `ThreadPool::push(Job)`→`expected` / `push(ThreadWorker)`→`void`, `KafkaQueueEmitter::send`→`DeliveryResult`(값).

5. **Logger는 유일 싱글턴.** `Logger::handle()`(참조 반환)로 접근, 설정을 먼저 하고 `start("파일베이스명")` 후에 `write()`가 실제로 기록된다. 종료는 `stop()` → `Logger::destroy()`. `set_title()` 같은 메서드는 없다.

6. **CMake 작성 규약은 고정이다.** 라이브러리/샘플 `CMakeLists.txt`는 `module_usage_guide.md` §5 템플릿을 그대로 따른다(`set(LIBRARY_NAME ...)` → `project` → `add_library` → `target_include_directories(... PUBLIC ...)` → `find_package`/`target_link_libraries`; 소스는 명시 나열, GLOB 금지). `vcpkg.json` 변경 후에는 반드시 `FRESH_DEPS=1 ./build.sh`.

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
| AWSService (S3, SQS) | AWSServiceSample (`BUILD_AWS=1`) | TC-7.1 ~ TC-7.6 |

새로운 기능을 추가할 때는 관련 샘플에 테스트 시나리오를 추가하고, `docs/sample_test_cases.md`에 테스트 케이스를 함께 업데이트한다.
