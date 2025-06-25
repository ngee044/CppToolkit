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
#include "Job.h"
#include "JobPriorities.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <numeric>
#include <fmt/format.h>
#include <fmt/core.h>

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

// DBBind implementations for type-safe database operations
class PlayerCreateBind : public DBBind<2, 1>
{
public:
	PlayerCreateBind(std::shared_ptr<DBConnection> connection)
		: DBBind<2, 1>(connection, L"INSERT INTO players (username, nickname) OUTPUT INSERTED.player_id VALUES (?, ?)")
	{
		bind_column(0, &player_id_);
	}

	auto set_player_data(const std::wstring& username, const std::wstring& nickname) -> std::tuple<bool, std::optional<std::string>>
	{
		if (username.length() >= std::size(username_) || nickname.length() >= std::size(nickname_))
		{
			return { false, "Username or nickname too long" };
		}
		
		wcscpy_s(username_, std::size(username_), username.c_str());
		wcscpy_s(nickname_, std::size(nickname_), nickname.c_str());
		
		bind_param(0, username_);
		bind_param(1, nickname_);
		
		return { true, std::nullopt };
	}

	auto get_created_player_id() const -> std::int64_t { return player_id_; }

private:
	WCHAR username_[51];
	WCHAR nickname_[51];
	std::int64_t player_id_ = 0;
};
class PlayerQueryBind : public DBBind<0, 6>
{
public:
	PlayerQueryBind(std::shared_ptr<DBConnection> connection)
		: DBBind<0, 6>(connection, L"SELECT player_id, username, nickname, level, experience, gold FROM players ORDER BY player_id")
	{
		bind_column(0, &player_id_);
		bind_column(1, username_, std::size(username_));
		bind_column(2, nickname_, std::size(nickname_));
		bind_column(3, &level_);
		bind_column(4, &experience_);
		bind_column(5, &gold_);
	}

	auto get_player_info() const -> PlayerInfo
	{
		PlayerInfo player;
		player.player_id = player_id_;
		player.username = std::wstring(username_);
		player.nickname = std::wstring(nickname_);
		player.level = level_;
		player.experience = experience_;
		player.gold = gold_;
		return player;
	}

private:
	std::int64_t player_id_;
	WCHAR username_[51];
	WCHAR nickname_[51];
	std::int32_t level_;
	std::int64_t experience_;
	std::int64_t gold_;
};

class SinglePlayerQueryBind : public DBBind<1, 6>
{
public:
	SinglePlayerQueryBind(std::shared_ptr<DBConnection> connection, std::int64_t player_id)
		: DBBind<1, 6>(connection, L"SELECT player_id, username, nickname, level, experience, gold FROM players WHERE player_id = ?")
		, search_id_(player_id)
	{
		bind_param(0, &search_id_);
		bind_column(0, &player_id_);
		bind_column(1, username_, std::size(username_));
		bind_column(2, nickname_, std::size(nickname_));
		bind_column(3, &level_);
		bind_column(4, &experience_);
		bind_column(5, &gold_);
	}

	auto get_player_info() const -> PlayerInfo
	{
		PlayerInfo player;
		player.player_id = player_id_;
		player.username = std::wstring(username_);
		player.nickname = std::wstring(nickname_);
		player.level = level_;
		player.experience = experience_;
		player.gold = gold_;
		return player;
	}

private:
	std::int64_t search_id_;
	std::int64_t player_id_;
	WCHAR username_[51];
	WCHAR nickname_[51];
	std::int32_t level_;
	std::int64_t experience_;
	std::int64_t gold_;
};
class GoldUpdateBind : public DBBind<2, 0>
{
public:
	GoldUpdateBind(std::shared_ptr<DBConnection> connection, bool is_deduct)
		: DBBind<2, 0>(connection, is_deduct ? 
			L"UPDATE players SET gold = gold - ? WHERE player_id = ? AND gold >= ?" :
			L"UPDATE players SET gold = gold + ? WHERE player_id = ?")
		, is_deduct_(is_deduct)
	{
	}

	auto set_update_data(std::int64_t amount, std::int64_t player_id, std::int64_t min_required = 0) -> void
	{
		amount_ = amount;
		player_id_ = player_id;
		min_required_ = min_required;
		
		bind_param(0, &amount_);
		bind_param(1, &player_id_);
		
		if (is_deduct_)
		{
			bind_param(2, &min_required_);
		}
	}

private:
	bool is_deduct_;
	std::int64_t amount_;
	std::int64_t player_id_;
	std::int64_t min_required_;
};

// Global variables for configuration
std::string connection_string_ = "Driver={SQL Server};Server=(localdb)\\MSSQLLocalDB;Database=GameDB_Sample;Trusted_Connection=yes;";
int connection_pool_size_ = 10;
int stress_test_threads_ = 4;
int stress_test_operations_ = 1000;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::Information;
LogTypes write_console_ = LogTypes::Information;
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
auto demo_stress_test(std::shared_ptr<DBConnectionPool> pool) -> void;
auto cleanup_test_data(std::shared_ptr<DBConnectionPool> pool) -> void;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("GameDatabaseSample");
	Logger::handle().write(LogTypes::Information, "=== GameDatabase Module Comprehensive Demo ===");

	try
	{
		// 1. Initialize Database Connection Pool
		auto connection_pool = std::make_shared<DBConnectionPool>();
		
		std::wstring wconn_str = Converter::to_wstring(connection_string_);
		auto [connect_success, connect_error] = connection_pool->connect(connection_pool_size_, wconn_str);
		if (!connect_success)
		{
			throw std::runtime_error("Failed to connect to database: " + connect_error.value_or("Unknown error"));
		}
		
		Logger::handle().write(LogTypes::Information, "✓ Connected to database with pool size: " + std::to_string(connection_pool_size_));
		// 2. Initialize Migration System
		auto migration = std::make_shared<DBMigration>(connection_pool);
		
		auto [init_success, init_error] = migration->initialize();
		if (!init_success)
		{
			throw std::runtime_error("Failed to initialize migration system: " + init_error.value_or("Unknown error"));
		}
		Logger::handle().write(LogTypes::Information, "✓ Migration system initialized");

		// Setup database schema
		auto [schema_success, schema_error] = setup_database_schema(migration);
		if (!schema_success)
		{
			throw std::runtime_error("Failed to setup database schema: " + schema_error.value_or("Unknown error"));
		}
		Logger::handle().write(LogTypes::Information, "✓ Database schema setup completed");

		// 3. Initialize Cache System
		Logger::handle().write(LogTypes::Information, "✓ Initializing Memory Cache...");
		auto cache = std::make_shared<DBCache>(); // Default memory cache backend
		Logger::handle().write(LogTypes::Information, "✓ Memory cache initialized");

		// Run demonstrations
		Logger::handle().write(LogTypes::Information, "\n=== Starting Demonstrations ===\n");

		// Demo 1: Connection Pool
		demo_connection_pool(connection_pool);

		// Demo 2: Player Operations with DBBind (Type-Safe)
		demo_player_operations(connection_pool);

		// Demo 3: Transaction Handling with RAII Guard
		demo_transaction_handling(connection_pool);

		// Demo 4: Smart Cache System
		demo_cache_system(connection_pool, cache);

		// Demo 5: Stored Procedures
		demo_stored_procedures(connection_pool);

		// Demo 6: Stress Test (Optional)
		if (stress_test_operations_ > 0)
		{
			demo_stress_test(connection_pool);
		}

		// Cleanup
		Logger::handle().write(LogTypes::Information, "\n=== Cleanup ===");
		cleanup_test_data(connection_pool);
		
		auto [cache_clear_success, cache_clear_error] = cache->clear();
		if (cache_clear_success)
		{
			Logger::handle().write(LogTypes::Information, "✓ Cache cleared");
		}
		
		connection_pool->clear();
		Logger::handle().write(LogTypes::Information, "✓ Connection pool closed");

		Logger::handle().write(LogTypes::Information, "\n=== All demonstrations completed successfully! ===");
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Critical error: {}", e.what()));
		Logger::handle().stop();
		Logger::destroy();
		return -1;
	}
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

	auto [reg1_success, reg1_error] = migration->register_migration(migration_v1);
	if (!reg1_success)
	{
		return { false, "Failed to register player table migration: " + reg1_error.value_or("Unknown error") };
	}

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

	auto [reg2_success, reg2_error] = migration->register_migration(migration_v2);
	if (!reg2_success)
	{
		return { false, "Failed to register items table migration: " + reg2_error.value_or("Unknown error") };
	}
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

	auto [reg3_success, reg3_error] = migration->register_migration(migration_v3);
	if (!reg3_success)
	{
		return { false, "Failed to register inventory table migration: " + reg3_error.value_or("Unknown error") };
	}

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

	auto [reg4_success, reg4_error] = migration->register_migration(migration_v4);
	if (!reg4_success)
	{
		return { false, "Failed to register stored procedure migration: " + reg4_error.value_or("Unknown error") };
	}

	// Run migrations to latest version
	return migration->migrate_to_latest();
}
auto demo_connection_pool(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 1: Connection Pool Management ---");
	
	try
	{
		// Demonstrate getting and returning connections
		std::vector<std::shared_ptr<DBConnection>> connections;
		Logger::handle().write(LogTypes::Information, "Getting multiple connections from pool...");
		
		for (int i = 0; i < 5; ++i)
		{
			auto conn = pool->pop();
			if (conn)
			{
				connections.push_back(conn);
				Logger::handle().write(LogTypes::Information, fmt::format("  ✓ Got connection {}", i + 1));
			}
			else
			{
				Logger::handle().write(LogTypes::Error, fmt::format("  ⚠ Failed to get connection {}", i + 1));
			}
		}

		// Execute simple query on each connection
		Logger::handle().write(LogTypes::Information, "Executing queries on connections...");
		for (size_t i = 0; i < connections.size(); ++i)
		{
			auto [success, error] = connections[i]->execute(L"SELECT 1 as test");
			if (success)
			{
				Logger::handle().write(LogTypes::Information, fmt::format("  ✓ Connection {} query successful", i + 1));
			}
			else
			{
				Logger::handle().write(LogTypes::Error, fmt::format("  ✗ Connection {} query failed: {}", i + 1, error.value_or("Unknown error")));
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
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Connection pool demo failed: {}", e.what()));
	}
}
auto demo_player_operations(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 2: Type-Safe Player Operations with DBBind ---");
	
	auto connection = pool->pop();
	if (!connection)
	{
		Logger::handle().write(LogTypes::Error, "Failed to get connection from pool");
		return;
	}

	try
	{
		// 1. Create players using type-safe DBBind
		Logger::handle().write(LogTypes::Information, "Creating players with type-safe binding...");
		std::vector<std::pair<std::wstring, std::wstring>> players = {
			{L"hero_001", L"영웅"},
			{L"warrior_002", L"전사"},
			{L"mage_003", L"마법사"},
			{L"archer_004", L"궁수"},
			{L"healer_005", L"힐러"}
		};

		for (const auto& [username, nickname] : players)
		{
			auto player_bind = std::make_unique<PlayerCreateBind>(connection);
			
			auto [bind_success, bind_error] = player_bind->set_player_data(username, nickname);
			if (!bind_success)
			{
				Logger::handle().write(LogTypes::Error, fmt::format("Failed to bind player data: {}", bind_error.value_or("Unknown error")));
				continue;
			}
			
			auto [exec_success, exec_error] = player_bind->execute();
			if (!exec_success)
			{
				Logger::handle().write(LogTypes::Error, fmt::format("Failed to execute player creation: {}", exec_error.value_or("Unknown error")));
				continue;
			}

			auto [fetch_success, fetch_error] = player_bind->fetch();
			if (fetch_success)
			{
				std::int64_t new_id = player_bind->get_created_player_id();
				Logger::handle().write(LogTypes::Information, 
					fmt::format("  ✓ Created player: {} ({}) - ID: {}", 
						Converter::to_string(username), 
						Converter::to_string(nickname),
						new_id
					)
				);
			}
			else
			{
				Logger::handle().write(LogTypes::Error, fmt::format("Failed to fetch created player: {}", fetch_error.value_or("Unknown error")));
			}
		}
		// 2. Query players using type-safe binding
		Logger::handle().write(LogTypes::Information, "\nQuerying player data with type-safe binding...");
		
		auto player_query = std::make_unique<PlayerQueryBind>(connection);
		auto [query_success, query_error] = player_query->execute();
		
		if (query_success)
		{
			Logger::handle().write(LogTypes::Information, "\nPlayer List:");
			Logger::handle().write(LogTypes::Information, "ID | Username     | Nickname | Level | Gold");
			Logger::handle().write(LogTypes::Information, "---|------------- |----------|-------|------");
			
			auto [fetch_success, fetch_error] = player_query->fetch();
			while (fetch_success)
			{
				auto player = player_query->get_player_info();
				Logger::handle().write(LogTypes::Information,
					fmt::format("{:2d} | {:12s} | {:8s} | {:5d} | {:5d}g",
						player.player_id,
						Converter::to_string(player.username),
						Converter::to_string(player.nickname),
						player.level,
						player.gold
					)
				);
				auto [next_success, next_error] = player_query->fetch();
				fetch_success = next_success;
			}
		}
		else
		{
			Logger::handle().write(LogTypes::Error, fmt::format("Failed to query players: {}", query_error.value_or("Unknown error")));
		}
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Player operations demo failed: {}", e.what()));
	}

	pool->push(connection);
}
auto demo_transaction_handling(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 3: RAII Transaction Handling ---");
	
	auto connection = pool->pop();
	if (!connection)
	{
		Logger::handle().write(LogTypes::Error, "Failed to get connection from pool");
		return;
	}

	// Scenario: Transfer gold between players (must be atomic)
	Logger::handle().write(LogTypes::Information, "Demonstrating gold transfer with RAII transaction guard...");
	
	auto transaction = std::make_shared<DBTransaction>(connection);
	
	std::int64_t sender_id = 1;
	std::int64_t receiver_id = 2;
	std::int64_t transfer_amount = 500;

	try
	{
		// Begin transaction with RAII guard
		TransactionGuard guard(transaction);
		
		auto [begin_success, begin_error] = transaction->begin();
		if (!begin_success)
		{
			throw std::runtime_error("Failed to begin transaction: " + begin_error.value_or("Unknown error"));
		}

		// Deduct from sender using type-safe binding
		auto deduct_bind = std::make_unique<GoldUpdateBind>(connection, true);  // true = deduct
		deduct_bind->set_update_data(transfer_amount, sender_id, transfer_amount);
		
		auto [deduct_success, deduct_error] = deduct_bind->execute();
		if (!deduct_success)
		{
			throw std::runtime_error("Failed to prepare deduct query: " + deduct_error.value_or("Unknown error"));
		}
		
		if (connection->row_count() == 0)
		{
			throw std::runtime_error("Insufficient gold for transfer");
		}

		// Add to receiver
		auto add_bind = std::make_unique<GoldUpdateBind>(connection, false);  // false = add
		add_bind->set_update_data(transfer_amount, receiver_id);
		
		auto [add_success, add_error] = add_bind->execute();
		if (!add_success)
		{
			throw std::runtime_error("Failed to prepare add query: " + add_error.value_or("Unknown error"));
		}

		// Explicit commit (RAII guard will auto-rollback if exception occurs)
		auto [commit_success, commit_error] = guard.commit();
		if (!commit_success)
		{
			throw std::runtime_error("Failed to commit transaction: " + commit_error.value_or("Unknown error"));
		}
		
		Logger::handle().write(LogTypes::Information, 
			fmt::format("✓ Successfully transferred {} gold from Player {} to Player {}", 
				transfer_amount, sender_id, receiver_id));
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, 
			fmt::format("Transaction failed: {} (RAII guard will auto-rollback)", e.what()));
	}

	pool->push(connection);
}
auto demo_cache_system(std::shared_ptr<DBConnectionPool> pool, std::shared_ptr<DBCache> cache) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 4: Smart Cache System ---");
	
	auto connection = pool->pop();
	if (!connection)
	{
		Logger::handle().write(LogTypes::Error, "Failed to get connection from pool");
		return;
	}

	try
	{
		// Smart cache query helper using cache_query function
		auto get_player_with_cache = [&](std::int64_t player_id) -> std::tuple<bool, std::optional<std::string>, std::optional<PlayerInfo>>
		{
			std::string cache_key = fmt::format("player:{}", player_id);
			
			return cache->cache_query<PlayerInfo>(
				cache_key,
				[&]() -> std::tuple<bool, std::optional<std::string>, PlayerInfo>
				{
					// Query from database using type-safe binding
					auto player_query = std::make_unique<SinglePlayerQueryBind>(connection, player_id);
					
					auto [exec_success, exec_error] = player_query->execute();
					if (!exec_success)
					{
						return { false, exec_error, PlayerInfo{} };
					}
					
					auto [fetch_success, fetch_error] = player_query->fetch();
					if (!fetch_success)
					{
						return { false, fetch_error, PlayerInfo{} };
					}
					
					return { true, std::nullopt, player_query->get_player_info() };
				},
				std::chrono::seconds(300)  // 5 minute TTL
			);
		};
		
		// Test cache miss -> cache hit performance
		Logger::handle().write(LogTypes::Information, "Testing cache performance (first call - cache miss):");
		
		std::vector<std::chrono::microseconds> first_call_times;
		for (int i = 1; i <= 3; ++i)
		{
			auto start = std::chrono::high_resolution_clock::now();
			auto [success, error, player_data] = get_player_with_cache(i);
			auto end = std::chrono::high_resolution_clock::now();
			
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			first_call_times.push_back(duration);
			
			if (success && player_data.has_value())
			{
				auto player = player_data.value();
				Logger::handle().write(LogTypes::Information, 
					fmt::format("  ✓ Player {}: {} ({}μs - DB)", 
						player.player_id,
						Converter::to_string(player.nickname),
						duration.count()));
			}
			else
			{
				Logger::handle().write(LogTypes::Error, 
					fmt::format("  ✗ Failed to get player {}: {}", i, error.value_or("Unknown error")));
			}
		}
		// Test cache hit performance
		Logger::handle().write(LogTypes::Information, "\nSecond call (should hit cache):");
		
		std::vector<std::chrono::microseconds> second_call_times;
		for (int i = 1; i <= 3; ++i)
		{
			auto start = std::chrono::high_resolution_clock::now();
			auto [success, error, player_data] = get_player_with_cache(i);
			auto end = std::chrono::high_resolution_clock::now();
			
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			second_call_times.push_back(duration);
			
			if (success && player_data.has_value())
			{
				auto player = player_data.value();
				Logger::handle().write(LogTypes::Information, 
					fmt::format("  ✓ Player {}: {} ({}μs - cached)", 
						player.player_id,
						Converter::to_string(player.nickname),
						duration.count()));
			}
		}
		
		// Calculate performance improvement
		auto avg_db_time = std::accumulate(first_call_times.begin(), first_call_times.end(), std::chrono::microseconds(0)) / first_call_times.size();
		auto avg_cache_time = std::accumulate(second_call_times.begin(), second_call_times.end(), std::chrono::microseconds(0)) / second_call_times.size();
		
		double speedup = static_cast<double>(avg_db_time.count()) / avg_cache_time.count();
		
		Logger::handle().write(LogTypes::Information, 
			fmt::format("Performance Summary:"));
		Logger::handle().write(LogTypes::Information, 
			fmt::format("  Average DB Time: {:.2f}μs", static_cast<double>(avg_db_time.count())));
		Logger::handle().write(LogTypes::Information, 
			fmt::format("  Average Cache Time: {:.2f}μs", static_cast<double>(avg_cache_time.count())));
		Logger::handle().write(LogTypes::Information, 
			fmt::format("  ✓ Cache is {:.1f}x faster", speedup));

		// Show cache statistics
		auto stats = cache->get_statistics();
		Logger::handle().write(LogTypes::Information, "\nCache Statistics:");
		Logger::handle().write(LogTypes::Information, fmt::format("  Total Hits: {}", stats.total_hits));
		Logger::handle().write(LogTypes::Information, fmt::format("  Total Misses: {}", stats.total_misses));
		Logger::handle().write(LogTypes::Information, fmt::format("  Hit Rate: {:.1f}%", stats.hit_rate * 100));
		Logger::handle().write(LogTypes::Information, fmt::format("  Entry Count: {}", stats.current_entry_count));
		Logger::handle().write(LogTypes::Information, fmt::format("  Memory Usage: {:.2f} KB", stats.current_size_bytes / 1024.0));
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Cache demo failed: {}", e.what()));
	}

	pool->push(connection);
}
auto demo_stored_procedures(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 5: Stored Procedures ---");
	
	auto connection = pool->pop();
	if (!connection)
	{
		Logger::handle().write(LogTypes::Error, "Failed to get connection from pool");
		return;
	}

	try
	{
		// Test experience/level up procedure
		Logger::handle().write(LogTypes::Information, "Testing experience and level up system...");
		
		auto sp_add_exp = std::make_shared<DBStoredProcedure>(connection, L"sp_add_experience");
		
		std::int64_t player_id = 1;
		std::int64_t exp_amount = 2500; // Should cause level up for level 1 player
		
		sp_add_exp->add_input_parameter(L"@player_id", player_id);
		sp_add_exp->add_input_parameter(L"@exp_amount", exp_amount);
		sp_add_exp->add_output_parameter(L"@new_level", SQL_INTEGER);
		sp_add_exp->add_output_parameter(L"@level_up", SQL_BIT);

		auto [exp_success, exp_error] = sp_add_exp->execute();
		if (exp_success)
		{
			std::int32_t new_level;
			bool level_up;
			
			auto [level_success, level_error] = sp_add_exp->get_output_parameter(L"@new_level", new_level);
			auto [levelup_success, levelup_error] = sp_add_exp->get_output_parameter(L"@level_up", level_up);
			
			if (level_success && levelup_success)
			{
				Logger::handle().write(LogTypes::Information, 
					fmt::format("  ✓ Added {} experience to Player {}", exp_amount, player_id));
				
				if (level_up)
				{
					Logger::handle().write(LogTypes::Information, 
						fmt::format("  ✓ LEVEL UP! Player {} is now level {}", player_id, new_level));
				}
				else
				{
					Logger::handle().write(LogTypes::Information, 
						fmt::format("  ✓ Player {} remains at level {}", player_id, new_level));
				}
			}
			else
			{
				Logger::handle().write(LogTypes::Error, "Failed to retrieve output parameters");
			}
		}
		else
		{
			Logger::handle().write(LogTypes::Error, 
				fmt::format("Failed to execute stored procedure: {}", exp_error.value_or("Unknown error")));
		}
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Stored procedure demo failed: {}", e.what()));
	}

	pool->push(connection);
}
auto demo_stress_test(std::shared_ptr<DBConnectionPool> pool) -> void
{
	Logger::handle().write(LogTypes::Information, "\n--- Demo 6: Concurrent Stress Test ---");
	
	Logger::handle().write(LogTypes::Information, 
		fmt::format("Running stress test with {} threads, {} operations per thread", 
			stress_test_threads_, stress_test_operations_));

	std::atomic<std::int32_t> success_count(0);
	std::atomic<std::int32_t> error_count(0);
	std::atomic<std::int32_t> completed_jobs(0);
	
	auto start_time = std::chrono::high_resolution_clock::now();
	
	// Create and start ThreadPool
	auto thread_pool = std::make_shared<ThreadPool>("StressTestPool");
	auto [start_success, start_error] = thread_pool->start();
	if (!start_success)
	{
		Logger::handle().write(LogTypes::Error, 
			fmt::format("Failed to start ThreadPool: {}", start_error.value_or("Unknown error")));
		return;
	}
	
	std::int32_t total_jobs = stress_test_threads_ * stress_test_operations_;
	
	// Create and submit jobs
	for (int thread_id = 0; thread_id < stress_test_threads_; ++thread_id)
	{
		for (int op = 0; op < stress_test_operations_; ++op)
		{
			auto job = std::make_shared<Job>(
				JobPriorities::Normal,
				[&, thread_id, op]() -> std::tuple<bool, std::optional<std::string>>
				{
					try
					{
						auto connection = pool->pop();
						if (!connection)
						{
							error_count++;
							completed_jobs++;
							return { false, "Failed to get connection from pool" };
						}

						// Perform simple player query
						auto player_query = std::make_unique<SinglePlayerQueryBind>(connection, 1);
						auto [exec_success, exec_error] = player_query->execute();
						
						bool operation_success = false;
						if (exec_success)
						{
							auto [fetch_success, fetch_error] = player_query->fetch();
							if (fetch_success)
							{
								success_count++;
								operation_success = true;
							}
							else
							{
								error_count++;
							}
						}
						else
						{
							error_count++;
						}
						
						pool->push(connection);
						completed_jobs++;
						
						return { operation_success, std::nullopt };
					}
					catch (const std::exception& e)
					{
						error_count++;
						completed_jobs++;
						return { false, e.what() };
					}
				},
				fmt::format("StressTestJob_{}_{}", thread_id, op)
			);
			
			auto [push_success, push_error] = thread_pool->push(job);
			if (!push_success)
			{
				Logger::handle().write(LogTypes::Error, 
					fmt::format("Failed to push job: {}", push_error.value_or("Unknown error")));
				error_count++;
				completed_jobs++;
			}
		}
	}
	
	// Wait for all jobs to complete
	Logger::handle().write(LogTypes::Information, "Waiting for all jobs to complete...");
	while (completed_jobs.load() < total_jobs)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
	
	double ops_per_second = (static_cast<double>(success_count) / duration.count()) * 1000;
	
	Logger::handle().write(LogTypes::Information, 
		fmt::format("\nStress Test Results:"));
	Logger::handle().write(LogTypes::Information, 
		fmt::format("  Total Operations: {}", total_jobs));
	Logger::handle().write(LogTypes::Information, 
		fmt::format("  Successful: {}", success_count.load()));
	Logger::handle().write(LogTypes::Information, 
		fmt::format("  Failed: {}", error_count.load()));
	Logger::handle().write(LogTypes::Information, 
		fmt::format("  Duration: {}ms", duration.count()));
	Logger::handle().write(LogTypes::Information, 
		fmt::format("  ✓ Throughput: {:.2f} ops/sec", ops_per_second));
	
	// Stop ThreadPool
	auto [stop_success, stop_error] = thread_pool->stop();
	if (!stop_success)
	{
		Logger::handle().write(LogTypes::Error, 
			fmt::format("Failed to stop ThreadPool: {}", stop_error.value_or("Unknown error")));
	}
}
auto cleanup_test_data(std::shared_ptr<DBConnectionPool> pool) -> void
{
	auto connection = pool->pop();
	if (!connection)
	{
		Logger::handle().write(LogTypes::Error, "Failed to get connection for cleanup");
		return;
	}

	try
	{
		// Clean up in reverse order of foreign key dependencies
		Logger::handle().write(LogTypes::Information, "Cleaning up test data...");
		
		auto [inv_success, inv_error] = connection->execute(L"DELETE FROM inventory");
		if (inv_success)
		{
			Logger::handle().write(LogTypes::Information, "  ✓ Inventory data cleared");
		}
		else
		{
			Logger::handle().write(LogTypes::Error, 
				fmt::format("  ⚠ Failed to clear inventory: {}", inv_error.value_or("Unknown error")));
		}
		
		auto [player_success, player_error] = connection->execute(L"DELETE FROM players");
		if (player_success)
		{
			Logger::handle().write(LogTypes::Information, "  ✓ Player data cleared");
		}
		else
		{
			Logger::handle().write(LogTypes::Error, 
				fmt::format("  ⚠ Failed to clear players: {}", player_error.value_or("Unknown error")));
		}
		
		// Keep items table as it contains reference data
		Logger::handle().write(LogTypes::Information, "✓ Test data cleanup completed (items table preserved)");
	}
	catch (const std::exception& e)
	{
		Logger::handle().write(LogTypes::Error, fmt::format("Cleanup failed: {}", e.what()));
	}

	pool->push(connection);
}