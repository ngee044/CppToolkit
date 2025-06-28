#include "DBMigration.h"
#include <DBTransaction.h>
#include <Logger.h>
#include <File.h>
#include <Converter.h>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#endif
#include <sstream>
#include <algorithm>
#include <regex>
#include <thread>
#include <iomanip>

namespace GameDatabase
{
    DBMigration::DBMigration(std::shared_ptr<DBConnectionPool> connection_pool)
        : connection_pool_(connection_pool)
        , is_initialized_(false)
    {
    }

    DBMigration::~DBMigration() = default;

    auto DBMigration::initialize() -> std::tuple<bool, std::optional<std::string>>
    {
        if (is_initialized_)
        {
            return { true, std::nullopt };
        }

        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection" };
        }

        DBTransaction transaction(connection);
        auto [begin_success, begin_error] = transaction.begin();
        if (!begin_success)
        {
            connection_pool_->push(connection);
            return { false, begin_error };
        }

        try
        {
            // Create migration history table
            std::wstring create_table_sql = L""
                L"IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='migration_history' AND xtype='U')\n"
                L"CREATE TABLE migration_history (\n"
                L"    version INT PRIMARY KEY,\n"
                L"    name NVARCHAR(255) NOT NULL,\n"
                L"    checksum VARCHAR(64) NOT NULL,\n"
                L"    applied_at DATETIME NOT NULL DEFAULT GETDATE(),\n"
                L"    execution_time_ms INT NOT NULL,\n"
                L"    success BIT NOT NULL,\n"
                L"    error_message NVARCHAR(MAX)\n"
                L")";

            auto [create_success, create_error] = connection->execute(create_table_sql);
            if (!create_success)
            {
                transaction.rollback();
                connection_pool_->push(connection);
                return { false, "Failed to create migration history table: " + 
                         (create_error ? *create_error : "Unknown error") };
            }

            // Create migration lock table
            std::wstring create_lock_sql = L""
                L"IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='migration_lock' AND xtype='U')\n"
                L"CREATE TABLE migration_lock (\n"
                L"    id INT PRIMARY KEY CHECK (id = 1),\n"
                L"    locked BIT NOT NULL DEFAULT 0,\n"
                L"    locked_by NVARCHAR(255),\n"
                L"    locked_at DATETIME\n"
                L")";

            auto [create_lock_success, create_lock_error] = connection->execute(create_lock_sql);
            if (!create_lock_success)
            {
                transaction.rollback();
                connection_pool_->push(connection);
                return { false, "Failed to create migration lock table: " + 
                         (create_lock_error ? *create_lock_error : "Unknown error") };
            }

            // Insert initial lock record if not exists
            std::wstring insert_lock_sql = L""
                L"IF NOT EXISTS (SELECT * FROM migration_lock WHERE id = 1)\n"
                L"INSERT INTO migration_lock (id, locked) VALUES (1, 0)";

            auto [insert_lock_success, insert_lock_error] = connection->execute(insert_lock_sql);
            if (!insert_lock_success)
            {
                transaction.rollback();
                connection_pool_->push(connection);
                return { false, "Failed to initialize migration lock: " + 
                         (insert_lock_error ? *insert_lock_error : "Unknown error") };
            }

            auto [commit_success, commit_error] = transaction.commit();
            if (!commit_success)
            {
                connection_pool_->push(connection);
                return { false, commit_error };
            }

            connection_pool_->push(connection);
            is_initialized_ = true;

            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "DBMigration initialized successfully");

            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            transaction.rollback();
            connection_pool_->push(connection);
            return { false, std::string("Exception during initialization: ") + e.what() };
        }
    }

    auto DBMigration::register_migration(const MigrationInfo& migration) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Validate migration
        if (migration.version == 0)
        {
            return { false, "Migration version cannot be 0" };
        }

        if (migration.name.empty())
        {
            return { false, "Migration name cannot be empty" };
        }

        if (migration.up_script.empty())
        {
            return { false, "Migration up script cannot be empty" };
        }

        // Calculate checksum
        std::string checksum = calculate_checksum(migration.up_script);

        // Check for duplicate version
        auto existing_it = migrations_.find(migration.version);
        if (existing_it != migrations_.end())
        {
            // Verify checksum matches
            if (existing_it->second.checksum != checksum)
            {
                return { false, "Migration version " + std::to_string(migration.version) + 
                        " already exists with different checksum" };
            }
        }

        // Store migration
        MigrationInfo stored_migration = migration;
        stored_migration.checksum = checksum;
        migrations_[migration.version] = stored_migration;

        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Registered migration: v" + std::to_string(migration.version) + " - " + migration.name);

        return { true, std::nullopt };
    }

    auto DBMigration::load_migrations_from_directory(const std::filesystem::path& directory_path) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!std::filesystem::exists(directory_path))
        {
            return { false, "Directory does not exist: " + directory_path.string() };
        }

        if (!std::filesystem::is_directory(directory_path))
        {
            return { false, "Path is not a directory: " + directory_path.string() };
        }

        // Pattern for migration files: V{version}__{name}.sql
        std::regex migration_pattern(R"(V(\d+)__(.+)\.sql)");

        for (const auto& entry : std::filesystem::directory_iterator(directory_path))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string filename = entry.path().filename().string();
            std::smatch matches;

            if (!std::regex_match(filename, matches, migration_pattern))
            {
                continue;
            }

            // Parse version and name
            uint32_t version = std::stoul(matches[1].str());
            std::string name = matches[2].str();

            // Read file content
            Utilities::File file;
            auto open_result = file.open(entry.path().string(), std::ios::in);
            if (!std::get<0>(open_result))
            {
                return { false, "Failed to open migration file: " + filename };
            }
            
            auto [read_lines_result, read_error] = file.read_lines();
            if (!read_lines_result)
            {
                return { false, "Failed to read migration file: " + filename };
            }
            
            std::string content;
            for (const auto& line : *read_lines_result)
            {
                content += line + "\n";
            }
            file.close();

            // Convert to wide string
            std::wstring wide_content = Utilities::Converter::to_wstring(content);

            // Check for down script (separated by -- DOWN marker)
            std::wstring up_script = wide_content;
            std::wstring down_script;

            size_t down_marker_pos = wide_content.find(L"-- DOWN");
            if (down_marker_pos != std::wstring::npos)
            {
                up_script = wide_content.substr(0, down_marker_pos);
                down_script = wide_content.substr(down_marker_pos + 7); // Skip "-- DOWN"
            }

            // Create migration info
            MigrationInfo migration;
            migration.version = version;
            migration.name = name;
            migration.description = "Loaded from " + filename;
            migration.up_script = up_script;
            migration.down_script = down_script;
            migration.created_at = std::chrono::system_clock::now();

            // Register migration
            auto [reg_success, reg_error] = register_migration(migration);
            if (!reg_success)
            {
                return { false, reg_error };
            }
        }

        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Loaded " + std::to_string(migrations_.size()) + " migrations from " + directory_path.string());

        return { true, std::nullopt };
    }

    auto DBMigration::get_current_version() -> std::tuple<bool, std::optional<std::string>, std::uint32_t>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection", 0 };
        }

        try
        {
            std::wstring query = L"SELECT MAX(version) FROM migration_history WHERE success = 1";
            
            auto [execute_success, execute_error] = connection->execute(query);
            if (!execute_success)
            {
                connection_pool_->push(connection);
                return { false, "Failed to query current version: " + 
                         (execute_error ? *execute_error : "Unknown error"), 0 };
            }

            uint32_t current_version = 0;
            auto [fetch_success, fetch_error] = connection->fetch();
            if (fetch_success)
            {
                if (!connection->is_null())
                {
                    auto [data_success, data_value, data_error] = connection->get_data<std::uint32_t>(0);
                    if (data_success && data_value.has_value())
                    {
                        current_version = data_value.value();
                    }
                }
            }

            connection_pool_->push(connection);
            return { true, std::nullopt, current_version };
        }
        catch (const std::exception& e)
        {
            connection_pool_->push(connection);
            return { false, std::string("Exception getting current version: ") + e.what(), 0 };
        }
    }

    auto DBMigration::migrate_to_latest() -> std::tuple<bool, std::optional<std::string>>
    {
        if (migrations_.empty())
        {
            return { true, "No migrations to apply" };
        }

        // Get highest version
        uint32_t latest_version = 0;
        for (const auto& [version, migration] : migrations_)
        {
            if (version > latest_version)
            {
                latest_version = version;
            }
        }

        return migrate_to_version(latest_version);
    }

    auto DBMigration::migrate_to_version(std::uint32_t target_version) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Get current version
        auto [get_success, get_error, current_version] = get_current_version();
        if (!get_success)
        {
            return { false, get_error };
        }

        if (current_version == target_version)
        {
            return { true, "Already at version " + std::to_string(target_version) };
        }

        // Acquire migration lock
        auto [lock_success, lock_error] = acquire_lock();
        if (!lock_success)
        {
            return { false, lock_error };
        }

        // Ensure we release lock on exit
        struct LockGuard
        {
            DBMigration* migration;
            ~LockGuard() { migration->release_lock(); }
        } lock_guard{this};

        // Determine migration direction
        bool is_upgrade = target_version > current_version;
        
        // Get migrations to apply
        std::vector<MigrationInfo> migrations_to_apply;
        
        if (is_upgrade)
        {
            for (const auto& [version, migration] : migrations_)
            {
                if (version > current_version && version <= target_version)
                {
                    migrations_to_apply.push_back(migration);
                }
            }
            
            // Sort by version ascending for upgrades
            std::sort(migrations_to_apply.begin(), migrations_to_apply.end(),
                [](const MigrationInfo& a, const MigrationInfo& b) {
                    return a.version < b.version;
                });
        }
        else
        {
            // Downgrade
            for (const auto& [version, migration] : migrations_)
            {
                if (version <= current_version && version > target_version)
                {
                    migrations_to_apply.push_back(migration);
                }
            }
            
            // Sort by version descending for downgrades
            std::sort(migrations_to_apply.begin(), migrations_to_apply.end(),
                [](const MigrationInfo& a, const MigrationInfo& b) {
                    return a.version > b.version;
                });
        }

        // Apply migrations
        for (const auto& migration : migrations_to_apply)
        {
            auto [apply_success, apply_error] = apply_migration(migration, is_upgrade);
            if (!apply_success)
            {
                return { false, apply_error };
            }
        }

        Utilities::Logger::handle().write(Utilities::LogTypes::Information,
            "Successfully migrated from version " + std::to_string(current_version) + 
            " to " + std::to_string(target_version));

        return { true, std::nullopt };
    }

    auto DBMigration::apply_migration(const MigrationInfo& migration, bool is_upgrade) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection" };
        }

        auto start_time = std::chrono::steady_clock::now();

        DBTransaction transaction(connection);
        auto [begin_success, begin_error] = transaction.begin();
        if (!begin_success)
        {
            connection_pool_->push(connection);
            return { false, begin_error };
        }

        try
        {
            // Execute migration script
            const std::wstring& script = is_upgrade ? migration.up_script : migration.down_script;
            
            if (script.empty() && !is_upgrade)
            {
                connection_pool_->push(connection);
                return { false, "No down script available for migration " + migration.name };
            }

            // Split script by GO statements
            std::vector<std::wstring> statements = split_sql_script(script);
            
            for (const auto& statement : statements)
            {
                if (!statement.empty())
                {
                    auto [stmt_success, stmt_error] = connection->execute(statement);
                    if (!stmt_success)
                    {
                        transaction.rollback();
                        connection_pool_->push(connection);
                        return { false, "Failed to execute migration script for " + migration.name + ": " +
                                 (stmt_error ? *stmt_error : "Unknown error") };
                    }
                }
            }

            // Record migration in history
            auto end_time = std::chrono::steady_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            std::wstring record_sql;
            if (is_upgrade)
            {
                record_sql = L"INSERT INTO migration_history (version, name, checksum, execution_time_ms, success) "
                           L"VALUES (?, ?, ?, ?, 1)";
            }
            else
            {
                record_sql = L"DELETE FROM migration_history WHERE version = ?";
            }

            // Use direct parameter binding instead of DBBind template
            connection->prepare(record_sql);
            
            if (is_upgrade)
            {
                SQLLEN ind1 = 0, ind2 = SQL_NTS, ind3 = SQL_NTS, ind4 = 0;
                
                int32_t version_int32 = static_cast<int32_t>(migration.version);
                connection->bind_param(1, &version_int32, &ind1);
                
                std::wstring name_w = Utilities::Converter::to_wstring(migration.name);
                connection->bind_param(2, const_cast<WCHAR*>(name_w.c_str()), &ind2);
                
                std::wstring checksum_w = Utilities::Converter::to_wstring(migration.checksum);
                connection->bind_param(3, const_cast<WCHAR*>(checksum_w.c_str()), &ind3);
                
                int exec_time = static_cast<int>(execution_time.count());
                connection->bind_param(4, &exec_time, &ind4);
            }
            else
            {
                SQLLEN ind1 = 0;
                int32_t version_int32 = static_cast<int32_t>(migration.version);
                connection->bind_param(1, &version_int32, &ind1);
            }

            auto [record_success, record_error] = connection->execute(record_sql);
            if (!record_success)
            {
                transaction.rollback();
                connection_pool_->push(connection);
                return { false, "Failed to record migration history: " + 
                         (record_error ? *record_error : "Unknown error") };
            }

            auto [commit_success, commit_error] = transaction.commit();
            if (!commit_success)
            {
                connection_pool_->push(connection);
                return { false, commit_error };
            }

            connection_pool_->push(connection);

            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                (is_upgrade ? "Applied" : "Rolled back") + std::string(" migration: v") + 
                std::to_string(migration.version) + " - " + migration.name + 
                " (" + std::to_string(execution_time.count()) + "ms)");

            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            transaction.rollback();
            connection_pool_->push(connection);
            return { false, std::string("Exception during migration: ") + e.what() };
        }
    }

    auto DBMigration::split_sql_script(const std::wstring& script) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> statements;
        std::wstringstream stream(script);
        std::wstring line;
        std::wstring current_statement;

        while (std::getline(stream, line))
        {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(L" \t\r\n"));
            line.erase(line.find_last_not_of(L" \t\r\n") + 1);

            // Check for GO statement
            if (line == L"GO" || line == L"go")
            {
                if (!current_statement.empty())
                {
                    statements.push_back(current_statement);
                    current_statement.clear();
                }
            }
            else
            {
                if (!current_statement.empty())
                {
                    current_statement += L"\n";
                }
                current_statement += line;
            }
        }

        // Add last statement
        if (!current_statement.empty())
        {
            statements.push_back(current_statement);
        }

        return statements;
    }

    auto DBMigration::calculate_checksum(const std::wstring& content) -> std::string
    {
        std::string utf8_content = Utilities::Converter::to_string(content);
        
#ifdef _WIN32
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS status = 0;
        DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
        PBYTE pbHashObject = nullptr;
        PBYTE pbHash = nullptr;

        // Open an algorithm handle
        if (!BCRYPT_SUCCESS(status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        {
            return "";
        }

        // Calculate the size of the buffer to hold the hash object
        if (!BCRYPT_SUCCESS(status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0)))
        {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // Allocate the hash object on the heap
        pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
        if (nullptr == pbHashObject)
        {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // Calculate the length of the hash
        if (!BCRYPT_SUCCESS(status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0)))
        {
            HeapFree(GetProcessHeap(), 0, pbHashObject);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // Allocate the hash buffer on the heap
        pbHash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHash);
        if (nullptr == pbHash)
        {
            HeapFree(GetProcessHeap(), 0, pbHashObject);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // Create a hash
        if (!BCRYPT_SUCCESS(status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, nullptr, 0, 0)))
        {
            HeapFree(GetProcessHeap(), 0, pbHash);
            HeapFree(GetProcessHeap(), 0, pbHashObject);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // Hash some data
        if (!BCRYPT_SUCCESS(status = BCryptHashData(hHash, (PBYTE)utf8_content.c_str(), utf8_content.length(), 0)))
        {
            BCryptDestroyHash(hHash);
            HeapFree(GetProcessHeap(), 0, pbHash);
            HeapFree(GetProcessHeap(), 0, pbHashObject);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // Close the hash
        if (!BCRYPT_SUCCESS(status = BCryptFinishHash(hHash, pbHash, cbHash, 0)))
        {
            BCryptDestroyHash(hHash);
            HeapFree(GetProcessHeap(), 0, pbHash);
            HeapFree(GetProcessHeap(), 0, pbHashObject);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        std::stringstream ss;
        for (DWORD i = 0; i < cbHash; i++)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(pbHash[i]);
        }

        BCryptDestroyHash(hHash);
        HeapFree(GetProcessHeap(), 0, pbHash);
        HeapFree(GetProcessHeap(), 0, pbHashObject);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        return ss.str();
#else
        // For non-Windows platforms, use a simple hash or return a fixed string
        std::hash<std::string> hasher;
        auto hash_value = hasher(utf8_content);
        
        std::stringstream ss;
        ss << std::hex << hash_value;
        return ss.str();
#endif
    }

    auto DBMigration::acquire_lock() -> std::tuple<bool, std::optional<std::string>>
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            return { false, "Failed to get database connection" };
        }

        try
        {
            // Try to acquire lock
            std::wstring lock_sql = L"UPDATE migration_lock SET locked = 1, locked_by = ?, locked_at = GETDATE() "
                                  L"WHERE id = 1 AND locked = 0";

            // Use direct parameter binding
            connection->prepare(lock_sql);
            
            // Create thread ID hash as string
            std::ostringstream thread_id_stream;
            thread_id_stream << std::this_thread::get_id();
            std::string thread_id_str = "DBMigration_" + thread_id_stream.str();
            std::wstring thread_id_w = Utilities::Converter::to_wstring(thread_id_str);
            
            SQLLEN ind1 = SQL_NTS;
            connection->bind_param(1, const_cast<WCHAR*>(thread_id_w.c_str()), &ind1);

            auto [lock_exec_success, lock_exec_error] = connection->execute(lock_sql);
            if (!lock_exec_success)
            {
                connection_pool_->push(connection);
                return { false, "Failed to acquire migration lock - another migration may be in progress: " +
                         (lock_exec_error ? *lock_exec_error : "Unknown error") };
            }

            // Check if we got the lock
            if (connection->get_affected_rows() == 0)
            {
                connection_pool_->push(connection);
                return { false, "Migration lock is already held by another process" };
            }

            connection_pool_->push(connection);
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            connection_pool_->push(connection);
            return { false, std::string("Exception acquiring lock: ") + e.what() };
        }
    }

    auto DBMigration::release_lock() -> void
    {
        auto connection = connection_pool_->pop();
        if (!connection)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Failed to get connection to release migration lock");
            return;
        }

        try
        {
            std::wstring unlock_sql = L"UPDATE migration_lock SET locked = 0, locked_by = NULL, locked_at = NULL "
                                    L"WHERE id = 1";

            connection->execute(unlock_sql);
        }
        catch (const std::exception& e)
        {
            Utilities::Logger::handle().write(Utilities::LogTypes::Error,
                "Failed to release migration lock: " + std::string(e.what()));
        }

        connection_pool_->push(connection);
    }
}
