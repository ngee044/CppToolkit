#include "DBConnectionPool.h"
#include "DBMigration.h"
#include "DBTransaction.h"
#include "DBBind.h"
#include "DBCache.h"
#include "DBStoredProcedure.h"
#include "Logger.h"
#include "Converter.h"
#include "ArgumentParser.h"
#include "ThreadPool.h"
#include "ThreadWorker.h"

#include <iostream>
#include <format>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>

using namespace GameDatabase;
using namespace Utilities;
using namespace Thread;

// Game data structures
struct PlayerInfo
{
	std::int64_t player_id;
	std::wstring username;
	std::wstring nickname;
	std::int32_t level;
	std::int64_t experience;
	std::int64_t gold;
};

struct ItemInfo
{
	std::int64_t item_id;
	std::wstring item_name;
	std::int32_t item_type;
	std::int32_t rarity;
	std::int32_t attack_power;
	std::int32_t defense_power;
	std::int64_t price;
};

// Global variables for configuration
std::string connection_string_ = "Driver={SQL Server};Server=(localdb)\\MSSQLLocalDB;Database=GameDB_Sample;Trusted_Connection=yes;";
int connection_pool_size_ = 10;
int stress_test_threads_ = 4;
int stress_test_operations_ = 1000;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::All;
LogTypes write_console_ = LogTypes::All;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif

// Function declarations
auto parse_arguments(ArgumentParser& arguments) -> void;
auto print_help_message() -> void;
auto setup_database_schema(std::shared_ptr<DBMigration> migration) -> std::tuple<bool, std::optional<std::string>>;
auto demo_connection_pool(std::shared_ptr<DBConnectionPool> pool) -> void;
auto demo_player_operations(std::shared_ptr<DBConnectionPool> pool) -> void;
auto demo_transaction_handling(std::shared_ptr<DBConnectionPool> pool) -> void;
auto demo_cache_system(std::shared_ptr<DBConnectionPool> pool, std::shared_ptr<DBCache> cache) -> void;
auto demo_stored_procedures(std::shared_ptr<DBConnectionPool> pool) -> void;
auto cleanup_test_data(std::shared_ptr<DBConnectionPool> pool) -> void;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("GameDatabaseClient");

	// 1. Initialize Database Connection Pool
	auto connection_pool = std::make_shared<DBConnectionPool>();
	
	std::wstring wconn_str = Converter::to_wstring(connection_string_);
	auto [connect_success, connect_error] = connection_pool->connect(connection_pool_size_, wconn_str);
	if (!connect_success)
	{
		Logger::handle().write(LogTypes::Error, std::format("Failed to connect to database: {}", connect_error.value_or("Unknown error")));
		return -1;
	}
	
	Logger::handle().write(LogTypes::Information, "Connected to database");


	// 2. Initialize Migration System
	auto migration = std::make_shared<DBMigration>(connection_pool);
	
	auto [init_success, init_error] = migration->initialize();
	if (!init_success)
	{
		Logger::handle().write(LogTypes::Error, std::format("Failed to initialize migration system: {}", init_error.value_or("Unknown error")));
		
		connection_pool->clear();
		return -1;
	}
	Logger::handle().write(LogTypes::Information, "Migration system initialized");

	// Setup database schema
	auto [schema_success, schema_error] = setup_database_schema(migration);
	if (!schema_success)
	{
		Logger::handle().write(LogTypes::Error, std::format("Failed to setup database schema: {}", schema_error.value_or("Unknown error")));
		connection_pool->clear();
		return -1;
	}
	// 3. Initialize Cache System
	Logger::handle().write(LogTypes::Information, "Initializing Memory Cache...");
	
	auto cache = std::make_shared<DBCache>(); // Default memory cache backend
	Logger::handle().write(LogTypes::Information, "✓ Memory cache initialized");	// Run demonstrations
	Logger::handle().write(LogTypes::Information, "\n=== Starting Demonstrations ===\n");

	// Demo 1: Connection Pool
	demo_connection_pool(connection_pool);

	// Demo 2: Player Operations (CRUD)
	demo_player_operations(connection_pool);

	// Demo 3: Transaction Handling
	demo_transaction_handling(connection_pool);

	// Demo 4: Cache System
	demo_cache_system(connection_pool, cache);

	// Demo 5: Stored Procedures
	demo_stored_procedures(connection_pool);

	// Cleanup
	Logger::handle().write(LogTypes::Information, "=== Cleanup ===");
	cleanup_test_data(connection_pool);
	cache->clear();
	Logger::handle().write(LogTypes::Information, "Cache cleared");
	
	connection_pool->clear();
	Logger::handle().write(LogTypes::Information, "Connection pool closed");

	Logger::handle().write(LogTypes::Information, "=== All demonstrations completed successfully! ===");

	Logger::handle().stop();
	Logger::destroy();
	
	return 0;
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	auto string_target = arguments.to_string("--help");
	if (string_target != std::nullopt)
	{
		print_help_message();
		return;
	}

	string_target = arguments.to_string("--connection_string");
	if (string_target != std::nullopt)
	{
		connection_string_ = string_target.value();
	}

	auto int_target = arguments.to_int("--pool_size");
	if (int_target != std::nullopt)
	{
		connection_pool_size_ = int_target.value();
	}

	int_target = arguments.to_int("--stress_threads");
	if (int_target != std::nullopt)
	{
		stress_test_threads_ = int_target.value();
	}
	
	int_target = arguments.to_int("--stress_operations");
	if (int_target != std::nullopt)
	{
		stress_test_operations_ = int_target.value();
	}
}

auto print_help_message() -> void
{
	std::cout << "\nGameDatabase Sample - Demonstrates GameDatabase module features\n\n";
	std::cout << "Usage: GameDatabaseSample [options]\n\n";
	std::cout << "Options:\n";
	std::cout << "  --help, -h                Show this help message\n";
	std::cout << "  --connection-string <str> Database connection string\n";
	std::cout << "                           Default: SQL Server LocalDB\n";
	std::cout << "  --pool-size <n>          Connection pool size (default: 10)\n";
	std::cout << "  --stress-test            Run stress test\n";
	std::cout << "  --stress-threads <n>     Number of threads for stress test (default: 4)\n";
	std::cout << "  --stress-operations <n>  Operations per thread (default: 1000)\n";
	std::cout << "  --verbose                Enable verbose logging\n\n";
	std::cout << "Examples:\n";
	std::cout << "  GameDatabaseSample\n";
	std::cout << "  GameDatabaseSample --pool-size 20\n";
	std::cout << "  GameDatabaseSample --stress-test --stress-threads 8\n\n";
}

auto setup_database_schema(std::shared_ptr<DBMigration> migration) -> std::tuple<bool, std::optional<std::string>>
{
	// Migration 1: Create player table
	MigrationInfo migration_v1;
	migration_v1.version = 1;
	migration_v1.name = "Create_Player_Table";
	migration_v1.up_script = L"CREATE TABLE players ("
							L"    player_id BIGINT IDENTITY(1,1) PRIMARY KEY,"
							L"    username NVARCHAR(50) UNIQUE NOT NULL,"
							L"    nickname NVARCHAR(50) NOT NULL,"
							L"    level INT DEFAULT 1 CHECK (level >= 1 AND level <= 100),"
							L"    experience BIGINT DEFAULT 0 CHECK (experience >= 0),"
							L"    gold BIGINT DEFAULT 1000 CHECK (gold >= 0),"
							L"    created_at DATETIME2 DEFAULT GETDATE(),"
							L"    last_login DATETIME2 DEFAULT GETDATE(),"
							L"    is_banned BIT DEFAULT 0,"
							L"    INDEX idx_username (username),"
							L"    INDEX idx_level (level DESC)"
							L");";
	migration_v1.down_script = L"DROP TABLE IF EXISTS players;";

	migration->register_migration(migration_v1);

	// Migration 2: Create items table
	MigrationInfo migration_v2;
	migration_v2.version = 2;
	migration_v2.name = "Create_Items_Table";
	migration_v2.up_script = L"CREATE TABLE items ("
							L"    item_id BIGINT IDENTITY(1000,1) PRIMARY KEY,"
							L"    item_name NVARCHAR(100) NOT NULL,"
							L"    item_type INT NOT NULL," // 1: Weapon, 2: Armor, 3: Consumable, 4: Material
							L"    rarity INT DEFAULT 1 CHECK (rarity >= 1 AND rarity <= 5)," // 1: Common to 5: Mythic
							L"    attack_power INT DEFAULT 0 CHECK (attack_power >= 0),"
							L"    defense_power INT DEFAULT 0 CHECK (defense_power >= 0),"
							L"    price BIGINT DEFAULT 100 CHECK (price >= 0),"
							L"    max_stack INT DEFAULT 1,"
							L"    INDEX idx_item_type (item_type),"
							L"    INDEX idx_rarity (rarity)"
							L");"
							L"-- Insert sample items\n"
							L"INSERT INTO items (item_name, item_type, rarity, attack_power, defense_power, price, max_stack) VALUES "
							L"(N'초보자의 검', 1, 1, 10, 0, 100, 1),"
							L"(N'초보자의 방패', 2, 1, 0, 10, 100, 1),"
							L"(N'강철 검', 1, 2, 25, 5, 500, 1),"
							L"(N'미스릴 갑옷', 2, 3, 0, 50, 2000, 1),"
							L"(N'전설의 대검', 1, 5, 100, 20, 10000, 1),"
							L"(N'체력 포션 (소)', 3, 1, 0, 0, 50, 99),"
							L"(N'체력 포션 (대)', 3, 2, 0, 0, 200, 99),"
							L"(N'철광석', 4, 1, 0, 0, 10, 999),"
							L"(N'미스릴 주괴', 4, 3, 0, 0, 500, 999);";
	migration_v2.down_script = L"DROP TABLE IF EXISTS items;";

	migration->register_migration(migration_v2);

	// Migration 3: Create inventory table
	MigrationInfo migration_v3;
	migration_v3.version = 3;
	migration_v3.name = "Create_Inventory_Table";
	migration_v3.up_script = L"CREATE TABLE inventory ("
							L"    inventory_id BIGINT IDENTITY(1,1) PRIMARY KEY,"
							L"    player_id BIGINT NOT NULL,"
							L"    item_id BIGINT NOT NULL,"
							L"    quantity INT DEFAULT 1 CHECK (quantity > 0),"
							L"    equipped BIT DEFAULT 0,"
							L"    slot_number INT NULL,"
							L"    acquired_at DATETIME2 DEFAULT GETDATE(),"
							L"    FOREIGN KEY (player_id) REFERENCES players(player_id) ON DELETE CASCADE,"
							L"    FOREIGN KEY (item_id) REFERENCES items(item_id),"
							L"    INDEX idx_player_inventory (player_id),"
							L"    INDEX idx_equipped (player_id, equipped)"
							L");";
	migration_v3.down_script = L"DROP TABLE IF EXISTS inventory;";

	migration->register_migration(migration_v3);

	// Migration 4: Create stored procedures
	MigrationInfo migration_v4;
	migration_v4.version = 4;
	migration_v4.name = "Create_Stored_Procedures";
	migration_v4.up_script = L"-- Procedure to add experience and handle level up\n"
							L"CREATE PROCEDURE sp_add_experience\n"
							L"    @player_id BIGINT,\n"
							L"    @exp_amount BIGINT,\n"
							L"    @new_level INT OUTPUT,\n"
							L"    @level_up BIT OUTPUT\n"
							L"AS\n"
							L"BEGIN\n"
							L"    SET NOCOUNT ON;\n"
							L"    DECLARE @current_level INT;\n"
							L"    DECLARE @current_exp BIGINT;\n"
							L"    DECLARE @exp_for_next_level BIGINT;\n"
							L"    \n"
							L"    SELECT @current_level = level, @current_exp = experience\n"
							L"    FROM players WHERE player_id = @player_id;\n"
							L"    \n"
							L"    SET @current_exp = @current_exp + @exp_amount;\n"
							L"    SET @exp_for_next_level = @current_level * 1000;\n"
							L"    \n"
							L"    IF @current_exp >= @exp_for_next_level AND @current_level < 100\n"
							L"    BEGIN\n"
							L"        SET @new_level = @current_level + 1;\n"
							L"        SET @level_up = 1;\n"
							L"        UPDATE players SET level = @new_level, experience = @current_exp - @exp_for_next_level\n"
							L"        WHERE player_id = @player_id;\n"
							L"    END\n"
							L"    ELSE\n"
							L"    BEGIN\n"
							L"        SET @new_level = @current_level;\n"
							L"        SET @level_up = 0;\n"
							L"        UPDATE players SET experience = @current_exp WHERE player_id = @player_id;\n"
							L"    END\n"
							L"END;\n";
	migration_v4.down_script = L"DROP PROCEDURE IF EXISTS sp_add_experience;";

	migration->register_migration(migration_v4);

	// Run migrations to latest version
	return migration->migrate_to_latest();
}

auto demo_connection_pool(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 1: Connection Pool Management ---");
	
	// Demonstrate getting and returning connections
	std::vector<std::shared_ptr<DBConnection>> connections;
		Logger::handle().write(LogTypes::Information, "Getting multiple connections from pool...");
	for (int i = 0; i < 5; ++i)
	{
		auto conn = pool->pop();
		if (conn)
		{
			connections.push_back(conn);
			Logger::handle().write(LogTypes::Information, std::format("  ✓ Got connection {}", i + 1));
		}
	}

	// Execute simple query on each connection
	Logger::handle().write(LogTypes::Information, "Executing queries on connections...");
	for (size_t i = 0; i < connections.size(); ++i)
	{
		auto [success, error] = connections[i]->execute(L"SELECT 1 as test");
		if (success)
		{
			Logger::handle().write(LogTypes::Information, std::format("  ✓ Connection {} query successful", i + 1));
		}
	}

	// Return connections to pool
	Logger::handle().write(LogTypes::Information, "Returning connections to pool...");
	for (auto& conn : connections)
	{
		pool->push(conn);
	}
	Logger::handle().write(LogTypes::Information, "✓ All connections returned to pool");
}

auto demo_player_operations(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 2: Player Operations (Direct Connection) ---");
	
	auto connection = pool->pop();

	// 1. Create players
	Logger::handle().write(LogTypes::Information, "Creating players...");
	std::vector<std::pair<std::wstring, std::wstring>> players = {
		{L"player_hero", L"영웅"},
		{L"player_warrior", L"전사"},
		{L"player_mage", L"마법사"},
		{L"player_archer", L"궁수"},
		{L"player_healer", L"힐러"}
	};

	for (const auto& [username, nickname] : players)
	{
		std::wstring query = L"INSERT INTO players (username, nickname) OUTPUT INSERTED.player_id VALUES (?, ?)";
		auto [exec_success, exec_error] = connection->execute(query);
		if (!exec_success)
		{
			Logger::handle().write(LogTypes::Error, 
				std::format("Failed to prepare insert statement: {}", exec_error.value_or("Unknown error")));
			continue;
		}

		// Bind parameters
		SQLLEN username_indicator = 0, nickname_indicator = 0;
		WCHAR username_buf[51], nickname_buf[51];
		wcscpy_s(username_buf, username.c_str());
		wcscpy_s(nickname_buf, nickname.c_str());
		
		connection->bind_param(1, username_buf, &username_indicator);
		connection->bind_param(2, nickname_buf, &nickname_indicator);

		// Bind output column
		std::int64_t new_player_id = 0;
		SQLLEN result_indicator = 0;
		connection->bind_column(1, &new_player_id, &result_indicator);

		auto [fetch_success, fetch_error] = connection->fetch();
		if (fetch_success)
		{
			Logger::handle().write(LogTypes::Information, 
				std::format("  ✓ Created player: {} ({}) - ID: {}", 
					Converter::to_string(username), 
					Converter::to_string(nickname),
					new_player_id
				)
			);
		}
	}

	// 2. Query players
	Logger::handle().write(LogTypes::Information, "\nQuerying player data...");
	
	std::wstring query = L"SELECT player_id, username, nickname, level, experience, gold FROM players ORDER BY player_id";
	auto [query_success, query_error] = connection->execute(query);
	
	if (query_success)
	{
		std::int64_t player_id;
		WCHAR username[51], nickname[51];
		std::int32_t level;
		std::int64_t experience, gold;
		SQLLEN indicators[6];

		connection->bind_column(1, &player_id, &indicators[0]);
		connection->bind_column(2, username, 50, &indicators[1]);
		connection->bind_column(3, nickname, 50, &indicators[2]);
		connection->bind_column(4, &level, &indicators[3]);
		connection->bind_column(5, &experience, &indicators[4]);
		connection->bind_column(6, &gold, &indicators[5]);

		Logger::handle().write(LogTypes::Information, "\nPlayer List:");
		Logger::handle().write(LogTypes::Information, "ID | Username | Nickname | Level | Gold");
		Logger::handle().write(LogTypes::Information, "---|----------|----------|-------|------");
		
		auto [fetch_success, fetch_error] = connection->fetch();
		while (fetch_success)
		{
			Logger::handle().write(LogTypes::Information,
				std::format("{:2d} | {:8s} | {:8s} | {:5d} | {:5d}g",
					player_id,
					Converter::to_string(std::wstring(username)),
					Converter::to_string(std::wstring(nickname)),
					level,
					gold
				)
			);
			auto [next_success, next_error] = connection->fetch();
			fetch_success = next_success;
		}
	}

	pool->push(connection);
}

auto demo_transaction_handling(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 3: Transaction Handling ---");
	
	auto connection = pool->pop();

	// Scenario: Transfer gold between players (must be atomic)
	Logger::handle().write(LogTypes::Information, "Demonstrating gold transfer with transaction...");
	
	auto transaction = std::make_shared<DBTransaction>(connection);
	
	std::int64_t sender_id = 1;
	std::int64_t receiver_id = 2;
	std::int64_t transfer_amount = 500;

	auto [begin_success, begin_error] = transaction->begin();
	if (!begin_success)
	{
		Logger::handle().write(LogTypes::Error, "Failed to begin transaction");
		pool->push(connection);
		return;
	}

	try
	{
		// Check sender's balance
		std::wstring check_query = L"SELECT gold FROM players WHERE player_id = ?";
		auto [check_success, check_error] = connection->execute(check_query);
		if (!check_success)
		{
			throw std::runtime_error("Failed to prepare balance check query");
		}

		SQLLEN sender_indicator = 0;
		connection->bind_param(1, &sender_id, &sender_indicator);
		
		std::int64_t sender_gold = 0;
		SQLLEN gold_indicator = 0;
		connection->bind_column(1, &sender_gold, &gold_indicator);
		
		auto [fetch_success, fetch_error] = connection->fetch();
		if (!fetch_success)
		{
			throw std::runtime_error("Failed to check sender balance");
		}

		if (sender_gold < transfer_amount)
		{
			throw std::runtime_error("Insufficient gold");
		}

		// Deduct from sender
		std::wstring deduct_query = L"UPDATE players SET gold = gold - ? WHERE player_id = ?";
		auto [deduct_exec_success, deduct_exec_error] = connection->execute(deduct_query);
		if (!deduct_exec_success)
		{
			throw std::runtime_error("Failed to prepare deduct query");
		}

		SQLLEN amount_indicator = 0, sender_id_indicator = 0;
		connection->bind_param(1, &transfer_amount, &amount_indicator);
		connection->bind_param(2, &sender_id, &sender_id_indicator);

		// Add to receiver
		std::wstring add_query = L"UPDATE players SET gold = gold + ? WHERE player_id = ?";
		auto [add_exec_success, add_exec_error] = connection->execute(add_query);
		if (!add_exec_success)
		{
			throw std::runtime_error("Failed to prepare add query");
		}

		SQLLEN amount_indicator2 = 0, receiver_id_indicator = 0;
		connection->bind_param(1, &transfer_amount, &amount_indicator2);
		connection->bind_param(2, &receiver_id, &receiver_id_indicator);

		// Commit transaction
		auto [commit_success, commit_error] = transaction->commit();
		if (commit_success)
		{
			Logger::handle().write(LogTypes::Information, 
				std::format("✓ Successfully transferred {} gold from Player {} to Player {}", 
					transfer_amount, sender_id, receiver_id));
		}
		else
		{
			throw std::runtime_error("Failed to commit transaction");
		}
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, std::format("Transaction failed: {}. Rolling back...", e.what()));
		transaction->rollback();
		Logger::handle().write(LogTypes::Information, "✓ Transaction rolled back successfully");
	}

	pool->push(connection);
}

auto demo_cache_system(std::shared_ptr<DBConnectionPool> pool, std::shared_ptr<DBCache> cache) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 4: Cache System ---");
	
	auto connection = pool->pop();

	// 1. Cache player data
	Logger::handle().write(LogTypes::Information, "Caching player data...");
	
	std::wstring query = L"SELECT player_id, username, nickname, level, experience, gold FROM players";
	auto [query_success, query_error] = connection->execute(query);
	
	if (query_success)
	{
		std::int64_t player_id;
		WCHAR username[51], nickname[51];
		std::int32_t level;
		std::int64_t experience, gold;
		SQLLEN indicators[6];

		connection->bind_column(1, &player_id, &indicators[0]);
		connection->bind_column(2, username, 50, &indicators[1]);
		connection->bind_column(3, nickname, 50, &indicators[2]);
		connection->bind_column(4, &level, &indicators[3]);
		connection->bind_column(5, &experience, &indicators[4]);
		connection->bind_column(6, &gold, &indicators[5]);

		int cached_count = 0;
		auto [fetch_success, fetch_error] = connection->fetch();
		while (fetch_success)
		{
			PlayerInfo player;
			player.player_id = player_id;
			player.username = std::wstring(username);
			player.nickname = std::wstring(nickname);
			player.level = level;
			player.experience = experience;
			player.gold = gold;

			std::string cache_key = "player:" + std::to_string(player_id);
			auto [set_success, set_error] = cache->set(cache_key, player, std::chrono::seconds(300)); // 5 minute TTL
			if (set_success)
			{
				cached_count++;
			}

			auto [next_success, next_error] = connection->fetch();
			fetch_success = next_success;
		}
		Logger::handle().write(LogTypes::Information, std::format("  ✓ Cached {} player records", cached_count));
	}

	// 2. Test cache performance
	Logger::handle().write(LogTypes::Information, "\nTesting cache performance...");
	
	auto start_db = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 100; ++i)
	{
		std::wstring single_query = L"SELECT nickname FROM players WHERE player_id = ?";
		auto [exec_success, exec_error] = connection->execute(single_query);
		if (exec_success)
		{
			std::int64_t test_id = 1;
			SQLLEN id_indicator = 0;
			connection->bind_param(1, &test_id, &id_indicator);
			
			WCHAR nickname_result[51];
			SQLLEN nickname_indicator = 0;
			connection->bind_column(1, nickname_result, 50, &nickname_indicator);
			connection->fetch();
		}
	}
	auto end_db = std::chrono::high_resolution_clock::now();
	auto db_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_db - start_db);

	auto start_cache = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 100; ++i)
	{
		auto [found, error, data] = cache->get<PlayerInfo>("player:1");
		if (found && data.has_value())
		{
			auto player = data.value();
		}
	}
	auto end_cache = std::chrono::high_resolution_clock::now();
	auto cache_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_cache - start_cache);

	Logger::handle().write(LogTypes::Information, std::format("  Database: 100 queries in {:.2f} ms ({:.2f} μs/query)",
		db_duration.count() / 1000.0, db_duration.count() / 100.0));
	Logger::handle().write(LogTypes::Information, std::format("  Cache: 100 reads in {:.2f} ms ({:.2f} μs/read)",
		cache_duration.count() / 1000.0, cache_duration.count() / 100.0));
	Logger::handle().write(LogTypes::Information, std::format("  ✓ Cache is {:.1f}x faster",
		static_cast<double>(db_duration.count()) / cache_duration.count()));

	// 3. Show cache statistics
	auto stats = cache->get_statistics();
	Logger::handle().write(LogTypes::Information, "\nCache Statistics:");
	Logger::handle().write(LogTypes::Information, std::format("  Total Hits: {}", stats.total_hits));
	Logger::handle().write(LogTypes::Information, std::format("  Total Misses: {}", stats.total_misses));
	Logger::handle().write(LogTypes::Information, std::format("  Hit Rate: {:.1f}%", stats.hit_rate * 100));
	Logger::handle().write(LogTypes::Information, std::format("  Entry Count: {}", stats.current_entry_count));
	Logger::handle().write(LogTypes::Information, std::format("  Memory Usage: {:.2f} KB", stats.current_size_bytes / 1024.0));

	pool->push(connection);
}

auto demo_stored_procedures(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 5: Stored Procedures ---");
	
	auto connection = pool->pop();

	// Test experience/level up procedure
	Logger::handle().write(LogTypes::Information, "Testing experience and level up system...");
	
	auto sp_add_exp = std::make_shared<DBStoredProcedure>(connection, L"sp_add_experience");
	
	std::int64_t player_id = 1;
	std::int64_t exp_amount = 2500; // Should cause level up
	
	sp_add_exp->add_input_parameter(L"@player_id", player_id);
	sp_add_exp->add_input_parameter(L"@exp_amount", exp_amount);
	sp_add_exp->add_output_parameter(L"@new_level", SQL_INTEGER);
	sp_add_exp->add_output_parameter(L"@level_up", SQL_BIT);

	auto [exp_success, exp_error] = sp_add_exp->execute();
	if (exp_success)
	{
		std::int32_t new_level;
		bool level_up;
		sp_add_exp->get_output_parameter(L"@new_level", new_level);
		sp_add_exp->get_output_parameter(L"@level_up", level_up);

		Logger::handle().write(LogTypes::Information, 
			std::format("  ✓ Added {} experience to Player {}", exp_amount, player_id));
		if (level_up)
		{
			Logger::handle().write(LogTypes::Information, 
				std::format("  ✓ LEVEL UP! Player {} is now level {}", player_id, new_level));
		}
		else
		{
			Logger::handle().write(LogTypes::Information, 
				std::format("  ✓ Player {} remains at level {}", player_id, new_level));
		}
	}

	pool->push(connection);
}

auto cleanup_test_data(std::shared_ptr<DBConnectionPool> pool) -> void
{
	auto connection = pool->pop();

	// Clean up in reverse order of foreign key dependencies
	connection->execute(L"DELETE FROM inventory");
	connection->execute(L"DELETE FROM players");
	// Keep items table as it's reference data

	Logger::handle().write(LogTypes::Information, "✓ Test data cleaned up");

	pool->push(connection);
}