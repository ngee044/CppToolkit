#include "DBMigration.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <functional>

namespace GameDataBase
{
    DBMigration::DBMigration(std::shared_ptr<DBConnectionPool> connection_pool)
        : connection_pool_(connection_pool)
    {
    }

    DBMigration::~DBMigration() = default;

    auto DBMigration::initialize() -> std::tuple<bool, std::optional<std::string>>
    {
        return create_migration_table();
    }

    auto DBMigration::register_migration(const MigrationInfo& migration) -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);
        
        if (migrations_.find(migration.version) != migrations_.end())
        {
            return { false, "Migration version already exists: " + std::to_string(migration.version) };
        }
        
        migrations_[migration.version] = migration;
        return { true, std::nullopt };
    }

    auto DBMigration::load_migrations_from_directory(const std::filesystem::path& directory_path) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!std::filesystem::exists(directory_path))
        {
            return { false, "Migration directory does not exist: " + directory_path.string() };
        }
        
        std::int32_t loaded_count = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator(directory_path))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".sql")
            {
                auto [success, error, migration] = parse_migration_file(entry.path());
                if (success)
                {
                    auto [reg_success, reg_error] = register_migration(migration);
                    if (reg_success)
                    {
                        loaded_count++;
                    }
                }
            }
        }
        
        return { true, std::nullopt };
    }

    auto DBMigration::get_current_version() -> std::tuple<bool, std::optional<std::string>, std::uint32_t>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection", 0 };
        }
        
        std::wstring query = L"SELECT MAX(version) FROM db_migration_history WHERE success = 1";
        auto [exec_success, exec_error] = connection->execute(query);
        if (!exec_success)
        {
            connection_pool_->push(connection);
            return { false, exec_error, 0 };
        }
        
        std::uint32_t current_version = 0;
        SQLLEN indicator = 0;
        connection->bind_column(1, &current_version, &indicator);
        
        auto [fetch_success, fetch_error] = connection->fetch();
        connection_pool_->push(connection);
        
        if (fetch_success && indicator != SQL_NULL_DATA)
        {
            return { true, std::nullopt, current_version };
        }
        
        return { true, std::nullopt, 0 };
    }

    auto DBMigration::migrate_to_version(std::uint32_t target_version) -> std::tuple<bool, std::optional<std::string>>
    {
        auto [current_success, current_error, current_version] = get_current_version();
        if (!current_success)
        {
            return { false, current_error };
        }
        
        if (current_version == target_version)
        {
            return { true, std::nullopt };
        }
        
        std::vector<std::uint32_t> versions_to_apply;
        
        // 업그레이드
        if (current_version < target_version)
        {
            for (const auto& [version, migration] : migrations_)
            {
                if (version > current_version && version <= target_version)
                {
                    versions_to_apply.push_back(version);
                }
            }
            std::sort(versions_to_apply.begin(), versions_to_apply.end());
        }
        // 다운그레이드
        else
        {
            for (const auto& [version, migration] : migrations_)
            {
                if (version <= current_version && version > target_version)
                {
                    versions_to_apply.push_back(version);
                }
            }
            std::sort(versions_to_apply.rbegin(), versions_to_apply.rend());
        }
        
        // 마이그레이션 실행
        for (std::uint32_t version : versions_to_apply)
        {
            if (before_migration_hook_)
            {
                before_migration_hook_(version);
            }
            
            auto& migration = migrations_[version];
            std::tuple<bool, std::optional<std::string>> result;
            
            if (current_version < target_version)
            {
                result = apply_migration(migration);
            }
            else
            {
                result = revert_migration(migration);
            }
            
            bool success = std::get<0>(result);
            
            if (after_migration_hook_)
            {
                after_migration_hook_(version, success);
            }
            
            if (!success)
            {
                return result;
            }
        }
        
        return { true, std::nullopt };
    }

    auto DBMigration::migrate_to_latest() -> std::tuple<bool, std::optional<std::string>>
    {
        if (migrations_.empty())
        {
            return { true, std::nullopt };
        }
        
        std::uint32_t latest_version = migrations_.rbegin()->first;
        return migrate_to_version(latest_version);
    }

    auto DBMigration::rollback_one() -> std::tuple<bool, std::optional<std::string>>
    {
        auto [success, error, current_version] = get_current_version();
        if (!success)
        {
            return { false, error };
        }
        
        if (current_version == 0)
        {
            return { false, "No migrations to rollback" };
        }
        
        // 이전 버전 찾기
        std::uint32_t previous_version = 0;
        for (const auto& [version, migration] : migrations_)
        {
            if (version < current_version && version > previous_version)
            {
                previous_version = version;
            }
        }
        
        return migrate_to_version(previous_version);
    }

    auto DBMigration::rollback_to_version(std::uint32_t target_version) -> std::tuple<bool, std::optional<std::string>>
    {
        return migrate_to_version(target_version);
    }

    auto DBMigration::get_migration_status(std::uint32_t version) -> MigrationStatus
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return MigrationStatus::FAILED;
        }
        
        std::wstring query = L"SELECT success FROM db_migration_history WHERE version = ? ORDER BY applied_at DESC";
        connection->execute(query);
        
        std::int32_t version_param = static_cast<std::int32_t>(version);
        SQLLEN indicator = 0;
        connection->bind_param(1, &version_param, &indicator);
        
        bool success = false;
        connection->bind_column(1, &success, &indicator);
        
        auto [fetch_success, fetch_error] = connection->fetch();
        connection_pool_->push(connection);
        
        if (!fetch_success)
        {
            return MigrationStatus::NOT_APPLIED;
        }
        
        return success ? MigrationStatus::APPLIED : MigrationStatus::FAILED;
    }

    auto DBMigration::get_migration_history() -> std::tuple<bool, std::optional<std::string>, std::vector<MigrationHistory>>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection", {} };
        }
        
        std::wstring query = L"SELECT version, name, applied_at, execution_time_ms, success, error_message "
                            L"FROM db_migration_history ORDER BY applied_at DESC";
        
        auto [exec_success, exec_error] = connection->execute(query);
        if (!exec_success)
        {
            connection_pool_->push(connection);
            return { false, exec_error, {} };
        }
        
        std::vector<MigrationHistory> history;
        
        while (true)
        {
            MigrationHistory entry;
            WCHAR name_buffer[256];
            WCHAR error_buffer[1024];
            TIMESTAMP_STRUCT timestamp;
            std::int32_t execution_time_ms;
            bool success;
            SQLLEN indicators[6];
            
            connection->bind_column(1, &entry.version, &indicators[0]);
            connection->bind_column(2, name_buffer, 256, &indicators[1]);
            connection->bind_column(3, &timestamp, &indicators[2]);
            connection->bind_column(4, &execution_time_ms, &indicators[3]);
            connection->bind_column(5, &success, &indicators[4]);
            connection->bind_column(6, error_buffer, 1024, &indicators[5]);
            
            auto [fetch_success, fetch_error] = connection->fetch();
            if (!fetch_success)
            {
                break;
            }
            
            entry.name = std::string(name_buffer, name_buffer + wcslen(name_buffer));
            entry.execution_time = std::chrono::milliseconds(execution_time_ms);
            entry.success = success;
            
            if (indicators[5] != SQL_NULL_DATA)
            {
                entry.error_message = std::string(error_buffer, error_buffer + wcslen(error_buffer));
            }
            
            history.push_back(entry);
        }
        
        connection_pool_->push(connection);
        return { true, std::nullopt, history };
    }

    auto DBMigration::get_pending_migrations() -> std::vector<MigrationInfo>
    {
        auto [success, error, current_version] = get_current_version();
        if (!success)
        {
            return {};
        }
        
        std::vector<MigrationInfo> pending;
        
        for (const auto& [version, migration] : migrations_)
        {
            if (version > current_version)
            {
                pending.push_back(migration);
            }
        }
        
        std::sort(pending.begin(), pending.end(), 
            [](const MigrationInfo& a, const MigrationInfo& b) {
                return a.version < b.version;
            });
            
        return pending;
    }

    auto DBMigration::validate_migration(std::uint32_t version) -> std::tuple<bool, std::optional<std::string>>
    {
        auto it = migrations_.find(version);
        if (it == migrations_.end())
        {
            return { false, "Migration not found: " + std::to_string(version) };
        }
        
        // 스크립트 구문 검증 (간단한 검사)
        const auto& migration = it->second;
        
        if (migration.up_script.empty())
        {
            return { false, "Up script is empty" };
        }
        
        if (migration.down_script.empty())
        {
            return { false, "Down script is empty" };
        }
        
        // 체크섬 검증
        std::string calculated_checksum = calculate_checksum(migration.up_script + migration.down_script);
        if (calculated_checksum != migration.checksum)
        {
            return { false, "Checksum mismatch" };
        }
        
        return { true, std::nullopt };
    }

    auto DBMigration::set_before_migration_hook(std::function<void(std::uint32_t)> hook) -> void
    {
        before_migration_hook_ = hook;
    }

    auto DBMigration::set_after_migration_hook(std::function<void(std::uint32_t, bool)> hook) -> void
    {
        after_migration_hook_ = hook;
    }

    auto DBMigration::create_migration_table() -> std::tuple<bool, std::optional<std::string>>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection" };
        }
        
        std::wstring create_table_sql = 
            L"IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='db_migration_history' AND xtype='U') "
            L"CREATE TABLE db_migration_history ("
            L"    id INT IDENTITY(1,1) PRIMARY KEY,"
            L"    version INT NOT NULL,"
            L"    name NVARCHAR(255) NOT NULL,"
            L"    applied_at DATETIME NOT NULL DEFAULT GETDATE(),"
            L"    execution_time_ms INT NOT NULL,"
            L"    success BIT NOT NULL,"
            L"    error_message NVARCHAR(MAX) NULL,"
            L"    checksum NVARCHAR(64) NOT NULL"
            L")";
        
        auto result = connection->execute(create_table_sql);
        connection_pool_->push(connection);
        
        return result;
    }

    auto DBMigration::apply_migration(const MigrationInfo& migration) -> std::tuple<bool, std::optional<std::string>>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection" };
        }
        
        auto transaction = std::make_shared<DBTransaction>(connection);
        TransactionGuard guard(transaction);
        
        auto start_time = std::chrono::steady_clock::now();
        
        auto [exec_success, exec_error] = execute_sql_script(connection, migration.up_script);
        
        auto end_time = std::chrono::steady_clock::now();
        auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        MigrationHistory history;
        history.version = migration.version;
        history.name = migration.name;
        history.applied_at = std::chrono::system_clock::now();
        history.execution_time = execution_time;
        history.success = exec_success;
        history.error_message = exec_error.value_or("");
        
        auto [record_success, record_error] = record_migration_history(history);
        if (!record_success)
        {
            connection_pool_->push(connection);
            return { false, record_error };
        }
        
        if (exec_success)
        {
            guard.commit();
        }
        
        connection_pool_->push(connection);
        return { exec_success, exec_error };
    }

    auto DBMigration::revert_migration(const MigrationInfo& migration) -> std::tuple<bool, std::optional<std::string>>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection" };
        }
        
        auto transaction = std::make_shared<DBTransaction>(connection);
        TransactionGuard guard(transaction);
        
        auto [exec_success, exec_error] = execute_sql_script(connection, migration.down_script);
        
        if (exec_success)
        {
            // 히스토리에서 제거
            std::wstring delete_sql = L"DELETE FROM db_migration_history WHERE version = ? AND success = 1";
            connection->execute(delete_sql);
            
            std::int32_t version_param = static_cast<std::int32_t>(migration.version);
            SQLLEN indicator = 0;
            connection->bind_param(1, &version_param, &indicator);
            
            guard.commit();
        }
        
        connection_pool_->push(connection);
        return { exec_success, exec_error };
    }

    auto DBMigration::record_migration_history(const MigrationHistory& history) -> std::tuple<bool, std::optional<std::string>>
    {
        // 이 메서드는 apply_migration 내에서 호출되므로 connection은 이미 있음
        // 실제 구현에서는 connection을 파라미터로 받아야 함
        return { true, std::nullopt };
    }

    auto DBMigration::parse_migration_file(const std::filesystem::path& file_path) 
        -> std::tuple<bool, std::optional<std::string>, MigrationInfo>
    {
        std::ifstream file(file_path);
        if (!file.is_open())
        {
            return { false, "Failed to open migration file", {} };
        }
        
        MigrationInfo migration;
        
        // 파일명에서 버전과 이름 추출 (예: V001__create_users_table.sql)
        std::string filename = file_path.stem().string();
        std::regex pattern(R"(V(\d+)__(.+))");
        std::smatch matches;
        
        if (!std::regex_match(filename, matches, pattern))
        {
            return { false, "Invalid migration filename format", {} };
        }
        
        migration.version = std::stoul(matches[1]);
        migration.name = matches[2];
        
        // 파일 내용 읽기
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        // UP/DOWN 섹션 분리
        std::regex up_pattern(R"(--\s*UP\s*\n([\s\S]*?)(?=--\s*DOWN|$))");
        std::regex down_pattern(R"(--\s*DOWN\s*\n([\s\S]*))");
        
        std::smatch up_match, down_match;
        if (std::regex_search(content, up_match, up_pattern))
        {
            migration.up_script = std::wstring(up_match[1].str().begin(), up_match[1].str().end());
        }
        
        if (std::regex_search(content, down_match, down_pattern))
        {
            migration.down_script = std::wstring(down_match[1].str().begin(), down_match[1].str().end());
        }
        
        migration.created_at = std::chrono::system_clock::now();
        migration.checksum = calculate_checksum(migration.up_script + migration.down_script);
        
        return { true, std::nullopt, migration };
    }

    auto DBMigration::calculate_checksum(const std::wstring& script) -> std::string
    {
        // 간단한 해시 함수 사용 (std::hash)
        std::hash<std::wstring> hasher;
        std::size_t hash_value = hasher(script);
        
        std::stringstream ss;
        ss << std::hex << hash_value;
        
        return ss.str();
    }

    auto DBMigration::execute_sql_script(std::shared_ptr<DBConnection> connection, const std::wstring& script) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // SQL 문장 분리 (세미콜론 기준)
        std::wistringstream stream(script);
        std::wstring statement;
        
        while (std::getline(stream, statement, L';'))
        {
            // 빈 문장 건너뛰기
            statement.erase(0, statement.find_first_not_of(L" \t\n\r"));
            statement.erase(statement.find_last_not_of(L" \t\n\r") + 1);
            
            if (statement.empty())
            {
                continue;
            }
            
            auto [success, error] = connection->execute(statement);
            if (!success)
            {
                return { false, error };
            }
        }
        
        return { true, std::nullopt };
    }

    // MigrationGenerator 구현
    auto MigrationGenerator::create_migration_file(
        const std::filesystem::path& directory,
        const std::string& name,
        const std::string& description) 
        -> std::tuple<bool, std::optional<std::string>, std::filesystem::path>
    {
        if (!std::filesystem::exists(directory))
        {
            std::filesystem::create_directories(directory);
        }
        
        // 타임스탬프 기반 버전 생성
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        std::stringstream version_stream;
        version_stream << std::put_time(&tm, "%Y%m%d%H%M%S");
        
        // 파일명 생성
        std::stringstream filename;
        filename << "V" << version_stream.str() << "__" << name << ".sql";
        
        std::filesystem::path file_path = directory / filename.str();
        
        // 템플릿 내용 생성
        std::uint32_t version = std::stoul(version_stream.str());
        std::string content = get_migration_template(version, name, description);
        
        // 파일 쓰기
        std::ofstream file(file_path);
        if (!file.is_open())
        {
            return { false, "Failed to create migration file", {} };
        }
        
        file << content;
        file.close();
        
        return { true, std::nullopt, file_path };
    }

    auto MigrationGenerator::get_migration_template(
        std::uint32_t version,
        const std::string& name,
        const std::string& description) -> std::string
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        std::stringstream template_content;
        
        template_content << "-- Migration: " << name << "\n";
        template_content << "-- Version: " << version << "\n";
        template_content << "-- Description: " << description << "\n";
        template_content << "-- Created: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n\n";
        
        template_content << "-- UP\n";
        template_content << "-- Add your upgrade SQL here\n\n";
        
        template_content << "-- DOWN\n";
        template_content << "-- Add your rollback SQL here\n";
        
        return template_content.str();
    }
}
