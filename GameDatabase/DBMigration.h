#pragma once

#include "DBConnection.h"
#include "DBTransaction.h"
#include "DBConnectionPool.h"
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <filesystem>
#include <map>

namespace GameDataBase
{
    // 마이그레이션 정보
    struct MigrationInfo
    {
        std::uint32_t version;
        std::string name;
        std::string description;
        std::wstring up_script;     // 업그레이드 SQL
        std::wstring down_script;   // 다운그레이드 SQL
        std::chrono::system_clock::time_point created_at;
        std::string checksum;       // 스크립트 무결성 확인용
    };

    // 마이그레이션 히스토리
    struct MigrationHistory
    {
        std::uint32_t version;
        std::string name;
        std::chrono::system_clock::time_point applied_at;
        std::chrono::milliseconds execution_time;
        bool success;
        std::string error_message;
    };

    // 마이그레이션 상태
    enum class MigrationStatus
    {
        NOT_APPLIED,
        APPLIED,
        FAILED,
        PENDING
    };

    // 마이그레이션 관리 클래스
    class DBMigration
    {
    public:
        DBMigration(std::shared_ptr<DBConnectionPool> connection_pool);
        ~DBMigration();

        // 마이그레이션 초기화 (히스토리 테이블 생성)
        auto initialize() -> std::tuple<bool, std::optional<std::string>>;

        // 마이그레이션 등록
        auto register_migration(const MigrationInfo& migration) -> std::tuple<bool, std::optional<std::string>>;
        
        // 파일에서 마이그레이션 로드
        auto load_migrations_from_directory(const std::filesystem::path& directory_path) 
            -> std::tuple<bool, std::optional<std::string>>;

        // 현재 DB 버전 확인
        auto get_current_version() -> std::tuple<bool, std::optional<std::string>, std::uint32_t>;

        // 특정 버전으로 마이그레이션
        auto migrate_to_version(std::uint32_t target_version) -> std::tuple<bool, std::optional<std::string>>;

        // 최신 버전으로 마이그레이션
        auto migrate_to_latest() -> std::tuple<bool, std::optional<std::string>>;

        // 한 버전 롤백
        auto rollback_one() -> std::tuple<bool, std::optional<std::string>>;

        // 특정 버전으로 롤백
        auto rollback_to_version(std::uint32_t target_version) -> std::tuple<bool, std::optional<std::string>>;

        // 마이그레이션 상태 확인
        auto get_migration_status(std::uint32_t version) -> MigrationStatus;

        // 마이그레이션 히스토리 조회
        auto get_migration_history() -> std::tuple<bool, std::optional<std::string>, std::vector<MigrationHistory>>;

        // 보류중인 마이그레이션 목록
        auto get_pending_migrations() -> std::vector<MigrationInfo>;

        // 마이그레이션 검증 (dry run)
        auto validate_migration(std::uint32_t version) -> std::tuple<bool, std::optional<std::string>>;

        // 마이그레이션 전후 훅
        auto set_before_migration_hook(std::function<void(std::uint32_t)> hook) -> void;
        auto set_after_migration_hook(std::function<void(std::uint32_t, bool)> hook) -> void;

    private:
        // 히스토리 테이블 생성
        auto create_migration_table() -> std::tuple<bool, std::optional<std::string>>;

        // 마이그레이션 실행
        auto apply_migration(const MigrationInfo& migration) -> std::tuple<bool, std::optional<std::string>>;

        // 마이그레이션 롤백
        auto revert_migration(const MigrationInfo& migration) -> std::tuple<bool, std::optional<std::string>>;

        // 히스토리 기록
        auto record_migration_history(const MigrationHistory& history) -> std::tuple<bool, std::optional<std::string>>;

        // 스크립트 파싱
        auto parse_migration_file(const std::filesystem::path& file_path) 
            -> std::tuple<bool, std::optional<std::string>, MigrationInfo>;

        // 체크섬 계산
        auto calculate_checksum(const std::wstring& script) -> std::string;

        // SQL 스크립트 실행 (여러 문장 지원)
        auto execute_sql_script(std::shared_ptr<DBConnection> connection, const std::wstring& script) 
            -> std::tuple<bool, std::optional<std::string>>;

    private:
        std::shared_ptr<DBConnectionPool> connection_pool_;
        std::map<std::uint32_t, MigrationInfo> migrations_;
        std::function<void(std::uint32_t)> before_migration_hook_;
        std::function<void(std::uint32_t, bool)> after_migration_hook_;
        mutable std::mutex migration_mutex_;
    };

    // 마이그레이션 파일 생성 헬퍼
    class MigrationGenerator
    {
    public:
        // 새 마이그레이션 파일 생성
        static auto create_migration_file(
            const std::filesystem::path& directory,
            const std::string& name,
            const std::string& description = "") 
            -> std::tuple<bool, std::optional<std::string>, std::filesystem::path>;

        // 마이그레이션 템플릿
        static auto get_migration_template(
            std::uint32_t version,
            const std::string& name,
            const std::string& description) -> std::string;
    };
}
