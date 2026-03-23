# 샘플 프로젝트 동작 테스트 케이스

CppToolkit 프레임워크 코드 변경 시, 아래 테스트 케이스를 통해 각 모듈의 정상 동작을 검증한다.
모든 샘플 실행파일은 빌드 후 `build/bin/` 에 위치한다.

---

## 1. ThreadSample

**검증 모듈:** ThreadPool, Job, ThreadWorker, Logger, ArgumentParser

### TC-1.1: ThreadPool 기본 생명주기 (생성 → 시작 → 작업 → 중지)

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/ThreadSample` |
| 기대 결과 | 10회 반복하여 ThreadPool 생성/시작/작업실행/중지가 에러 없이 완료된다 |
| 확인 포인트 | `High: 0` ~ `High: 9` 로그가 Information 레벨로 출력된다 |

### TC-1.2: 반복 횟수 커스텀

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/ThreadSample --max_count 3` |
| 기대 결과 | 3회만 반복 후 정상 종료 |
| 확인 포인트 | `High: 0` ~ `High: 2` 까지만 출력된다 |

### TC-1.3: Worker 제거 동작

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/ThreadSample` |
| 기대 결과 | Normal 우선순위 Worker가 `remove_workers`로 제거된 후에도 High 우선순위 Job이 정상 실행된다 |
| 확인 포인트 | Error 로그 없이 `High:` 로그가 출력된다 |

### TC-1.4: 로그 레벨 인자 파싱

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/ThreadSample --write_console_log 6 --max_count 1` |
| 기대 결과 | 콘솔 로그 레벨이 Debug로 변경되어 더 상세한 로그가 출력된다 |
| 확인 포인트 | 프로그램이 정상 종료되며 ArgumentParser가 값을 올바르게 파싱한다 |

---

## 2. NetworkServerSample + NetworkClientSample

**검증 모듈:** NetworkServer, NetworkClient, NetworkSession, DataHandler, ThreadPool, Converter

> 서버와 클라이언트를 별도 터미널에서 함께 실행해야 한다.

### TC-2.1: 기본 연결 및 메시지 에코

| 항목 | 내용 |
|------|------|
| 실행 | 터미널1: `./build/bin/NetworkServerSample` <br> 터미널2: `./build/bin/NetworkClientSample` |
| 기대 결과 | 클라이언트가 서버(127.0.0.1:9090)에 접속하고, 메시지/바이너리 에코 루프가 동작한다 |
| 확인 포인트 - 서버 | `received condition message` 로그에 연결 상태 출력, `received_message: echo` 로그 출력 |
| 확인 포인트 - 클라이언트 | `received condition of connection: true` 로그, `received_binary: echo` 로그가 반복 출력 |

### TC-2.2: 메시지 → 바이너리 → 메시지 에코 체인

| 항목 | 내용 |
|------|------|
| 실행 | TC-2.1과 동일 |
| 기대 결과 | 클라이언트: `send_message("echo")` → 서버: `send_binary()` 응답 → 클라이언트: `send_message()` 응답 → 반복 |
| 확인 포인트 | 서버에 `received_message`와 `received_binary` 로그가 번갈아 출력된다 |

### TC-2.3: 커스텀 포트 및 버퍼 설정

| 항목 | 내용 |
|------|------|
| 실행 | 터미널1: `./build/bin/NetworkServerSample --server_port 8080` <br> 터미널2: `./build/bin/NetworkClientSample --server_ip 127.0.0.1 --server_port 8080` |
| 기대 결과 | 포트 8080에서 정상 통신이 이루어진다 |
| 확인 포인트 | TC-2.1과 동일한 로그가 출력된다 |

### TC-2.4: Heartbeat 동작 검증

| 항목 | 내용 |
|------|------|
| 실행 | 터미널1: `./build/bin/NetworkServerSample --heartbeat_enable true --heartbeat_interval 2 --heartbeat_missed 2` <br> 터미널2: `./build/bin/NetworkClientSample --pong_enable true` |
| 기대 결과 | 서버가 2초 간격으로 heartbeat를 보내고, 클라이언트가 pong 응답을 보낸다 |
| 확인 포인트 | 연결이 유지되며 heartbeat 타임아웃이 발생하지 않는다 |

### TC-2.5: Heartbeat 타임아웃 세션 정리

| 항목 | 내용 |
|------|------|
| 실행 | 터미널1: `./build/bin/NetworkServerSample --heartbeat_enable true --heartbeat_interval 2 --heartbeat_missed 2` <br> 터미널2: `./build/bin/NetworkClientSample --pong_enable false` |
| 기대 결과 | 클라이언트가 pong을 보내지 않아 서버가 약 4초(2초 x 2회) 후 세션을 끊는다 |
| 확인 포인트 | 서버에서 세션 정리 로그, 클라이언트에서 `received condition of connection: false` 로그가 출력된다 |

### TC-2.6: 서버 미기동 상태에서 클라이언트 재접속

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/NetworkClientSample` (서버 미실행 상태) |
| 기대 결과 | `cannot connect to server : 127.0.0.1:9090` 에러 로그를 출력하며 1초 간격으로 재접속을 시도한다 |
| 확인 포인트 | 크래시 없이 재접속 루프가 동작한다 |

### TC-2.7: SIGINT 시그널 정상 종료

| 항목 | 내용 |
|------|------|
| 실행 | 서버 또는 클라이언트 실행 중 `Ctrl+C` 입력 |
| 기대 결과 | `stop()` → `wait_stop()` 흐름으로 정상 종료되고, Logger가 stop/destroy 된다 |
| 확인 포인트 | 프로세스가 크래시 없이 종료된다 |

### TC-2.8: Worker 스레드 수 설정

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/NetworkServerSample --high_priority_count 1 --normal_priority_count 1 --low_priority_count 1` |
| 기대 결과 | 최소 워커 구성에서도 서버가 정상 동작한다 |
| 확인 포인트 | 클라이언트 연결 시 TC-2.1과 동일하게 동작한다 |

---

## 3. DatabaseSample

**검증 모듈:** PostgresDB, DatabaseInterface, File, ArgumentParser

> PostgreSQL 서버가 실행 중이어야 한다.

### TC-3.1: SELECT 쿼리 실행

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/DatabaseSample --user <user> --dbname <db> --password <pass> --host localhost --port 5432 --query "SELECT 1 AS test" --operation select` |
| 기대 결과 | 쿼리 결과가 `Row:` 로그로 출력된다 |
| 확인 포인트 | `Row: 1` 형태의 출력이 확인된다 |

### TC-3.2: INSERT/UPDATE/DELETE 쿼리 실행

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/DatabaseSample --user <user> --dbname <db> --password <pass> --query "INSERT INTO test_table VALUES (1, 'data')" --operation insert` |
| 기대 결과 | `Query executed successfully.` 로그가 출력된다 |
| 확인 포인트 | Error 로그가 출력되지 않는다 |

### TC-3.3: 쿼리 파일에서 읽기

| 항목 | 내용 |
|------|------|
| 사전 조건 | `echo "SELECT 1 AS test" > /tmp/test_query.sql` |
| 실행 | `./build/bin/DatabaseSample --user <user> --dbname <db> --password <pass> --query_file /tmp/test_query.sql --operation select` |
| 기대 결과 | 파일에서 쿼리를 읽어 실행하고 결과를 출력한다 |
| 확인 포인트 | TC-3.1과 동일한 결과가 출력된다 |

### TC-3.4: 결과를 파일에 저장

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/DatabaseSample --user <user> --dbname <db> --password <pass> --query "SELECT 1 AS test" --output_file /tmp/db_result.txt` |
| 기대 결과 | 쿼리 결과가 `/tmp/db_result.txt`에 기록된다 |
| 확인 포인트 | `Results written to file:` 로그 출력, 파일 내용 확인 |

### TC-3.5: 필수 인자 누락 시 도움말 출력

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/DatabaseSample` |
| 기대 결과 | `Error: Invalid database connection details.` 출력 후 help 메시지가 표시된다 |
| 확인 포인트 | 크래시 없이 종료된다 |

### TC-3.6: 잘못된 연결 정보로 실행

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/DatabaseSample --user wrong --dbname wrong --password wrong --query "SELECT 1"` |
| 기대 결과 | `Query execution failed:` Error 로그가 출력된다 |
| 확인 포인트 | 크래시 없이 에러를 처리하고 종료된다 |

---

## 4. FolderWatcherSample

**검증 모듈:** FolderWatcher, Folder, File, Converter

### TC-4.1: 파일 시스템 이벤트 감지

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/FolderWatcherSample` |
| 기대 결과 | 임시 디렉토리에 폴더/파일을 생성하면서 FolderWatcher가 이벤트를 감지한다 |
| 확인 포인트 - 디렉토리 생성 | `create temp directory:` 로그 출력 |
| 확인 포인트 - 파일 생성 감지 | `has added on` 로그가 출력된다 (test.json 파일 생성 감지) |
| 확인 포인트 - 하위 폴더 감지 | `test_folder` 하위 디렉토리의 파일 변경도 감지된다 (recursive 모드) |

### TC-4.2: 정상 종료

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/FolderWatcherSample` |
| 기대 결과 | FolderWatcher.stop() → Logger.stop() → Logger.destroy() 순서로 정상 종료된다 |
| 확인 포인트 | 프로세스 종료 코드 0, 크래시 없음 |

### TC-4.3: File/Folder 유틸리티 동작

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/FolderWatcherSample` |
| 기대 결과 | Folder.create_folder(), File.open(), File.write_bytes(), File.close()가 모두 성공한다 |
| 확인 포인트 | Error 로그 없이 `create temp directory`, `create sub temp directory`, `create temp file` 로그가 순서대로 출력된다 |

---

## 5. FolderCompressionSample

**검증 모듈:** Folder (compression/decompression), File, ArgumentParser

### TC-5.1: 폴더 압축

| 항목 | 내용 |
|------|------|
| 사전 조건 | 테스트용 폴더 생성: `mkdir -p /tmp/compress_test && echo "test" > /tmp/compress_test/a.txt` |
| 실행 | `./build/bin/FolderCompressionSample --compress true --source /tmp/compress_test --compressed_file /tmp/compressed.dat` |
| 기대 결과 | `Successfully compressed folder to:` 로그가 출력된다 |
| 확인 포인트 | `/tmp/compressed.dat` 파일이 생성되고 크기가 0보다 크다 |

### TC-5.2: 파일 압축 해제

| 항목 | 내용 |
|------|------|
| 사전 조건 | TC-5.1에서 생성된 `/tmp/compressed.dat` 파일 |
| 실행 | `./build/bin/FolderCompressionSample --decompress true --compressed_file /tmp/compressed.dat --destination /tmp/decompress_test` |
| 기대 결과 | `Successfully decompressed file to:` 로그가 출력된다 |
| 확인 포인트 | `/tmp/decompress_test/` 에 원본 파일(`a.txt`)이 복원된다 |

### TC-5.3: 압축 → 해제 라운드트립

| 항목 | 내용 |
|------|------|
| 실행 | TC-5.1 실행 후 TC-5.2 실행 |
| 기대 결과 | 해제된 파일 내용이 원본과 동일하다 |
| 확인 포인트 | `diff /tmp/compress_test/a.txt /tmp/decompress_test/a.txt` 결과가 동일 |

### TC-5.4: 특정 확장자만 압축

| 항목 | 내용 |
|------|------|
| 사전 조건 | `echo "json" > /tmp/compress_test/b.json && echo "xml" > /tmp/compress_test/c.xml` |
| 실행 | `./build/bin/FolderCompressionSample --compress true --source /tmp/compress_test --compressed_file /tmp/compressed_ext.dat --file_extensions txt,json` |
| 기대 결과 | txt, json 파일만 압축에 포함된다 |
| 확인 포인트 | 해제 시 a.txt, b.json만 존재하고 c.xml은 없다 |

### TC-5.5: 존재하지 않는 소스 폴더

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/FolderCompressionSample --compress true --source /tmp/nonexistent --compressed_file /tmp/out.dat` |
| 기대 결과 | `Source folder does not exist:` Error 로그가 출력된다 |
| 확인 포인트 | 크래시 없이 종료된다 |

### TC-5.6: 모드 미지정 시 도움말 출력

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/FolderCompressionSample` |
| 기대 결과 | `FolderCompression Usage:` 도움말이 출력된다 |
| 확인 포인트 | 크래시 없이 종료된다 |

---

## 6. KafkaSample

**검증 모듈:** KafkaQueueEmitter, KafkaQueueConsume, KafkaConfig, KafkaMessage

> Kafka 브로커가 `127.0.0.1:9092`에서 실행 중이어야 한다.

### TC-6.1: 메시지 produce 및 consume

| 항목 | 내용 |
|------|------|
| 사전 조건 | Kafka 브로커 실행, `test_topic` 토픽 존재 (auto-create 또는 수동 생성) |
| 실행 | `./build/bin/KafkaSample` |
| 기대 결과 | Producer가 메시지를 전송하고, Consumer가 해당 메시지를 수신한다 |
| 확인 포인트 - Producer | `Message queued successfully.` 출력 |
| 확인 포인트 - Consumer | `[Consumer] Received: topic=test_topic, key=myKey, value=Hello Kafka from sample code!` 출력 |

### TC-6.2: Producer 시작 실패 처리

| 항목 | 내용 |
|------|------|
| 사전 조건 | Kafka 브로커 미실행 |
| 실행 | `./build/bin/KafkaSample` |
| 기대 결과 | `Producer start failed:` 에러 메시지 출력 후 종료 |
| 확인 포인트 | 크래시 없이 종료 코드 -1 반환 |

### TC-6.3: Consumer subscribe 및 poll

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/KafkaSample` (Kafka 정상 실행 상태) |
| 기대 결과 | Consumer가 `test_topic`에 subscribe 후 2초 동안 polling하여 메시지를 수신한다 |
| 확인 포인트 | `Polling messages...` 출력 후 수신된 메시지의 topic, key, value, partition, timestamp이 출력된다 |

### TC-6.4: 정상 종료 (flush → stop)

| 항목 | 내용 |
|------|------|
| 실행 | `./build/bin/KafkaSample` (Kafka 정상 실행 상태) |
| 기대 결과 | Producer flush → stop, Consumer stop 순서로 정상 종료된다 |
| 확인 포인트 | `Sample main completed.` 출력, 프로세스 종료 코드 0 |

---

## 공통 검증 항목

모든 샘플에 대해 아래 항목을 추가로 확인한다.

### Logger 생명주기

| 항목 | 내용 |
|------|------|
| 확인 포인트 | `Logger::handle().start()` → 작업 수행 → `Logger::handle().stop()` → `Logger::destroy()` 순서가 지켜진다 |
| 실패 시 | 로그가 출력되지 않거나, 종료 시 크래시/hang이 발생한다 |

### ArgumentParser 공통

| 항목 | 내용 |
|------|------|
| 확인 포인트 | `--write_console_log`, `--write_file_log` 인자가 모든 샘플에서 동작한다 |
| 실패 시 | 로그 레벨이 변경되지 않는다 |

### 메모리 안정성

| 항목 | 내용 |
|------|------|
| 확인 포인트 | 프로세스 종료 시 shared_ptr/unique_ptr이 정상 해제되어 메모리 누수가 없다 |
| 검증 방법 | macOS: `leaks --atExit -- ./build/bin/<Sample>` 또는 Instruments Allocations |

### 시그널 처리 (NetworkServerSample, NetworkClientSample)

| 항목 | 내용 |
|------|------|
| 확인 포인트 | SIGINT, SIGTERM 수신 시 graceful shutdown이 수행된다 |
| 검증 방법 | 실행 중 `kill -SIGINT <pid>` 또는 `Ctrl+C` |
