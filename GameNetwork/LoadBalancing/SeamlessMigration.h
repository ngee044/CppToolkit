#pragma once

#include <GameNetworkConstants.h>
#include "LoadBalancer.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <functional>
#include <mutex>
#include <tuple>
#include <optional>
#include <atomic>
#include <queue>

namespace GameNetwork
{
	// Forward declarations
	class GameSession;
	class LoadBalancer;
    
	enum class MigrationPhase
	{
		Preparing,
		StateSnapshot,
		StateTransfer,
		ConnectionHandover,
		Verification,
		Executing,
		Paused,
		Completing,
		Completed,
		Failed
	};
    
	struct MigrationTask
	{
		std::string task_id;
		std::string session_id;
		std::string source_server;
		std::string target_server;
		MigrationPhase phase;
		std::chrono::steady_clock::time_point start_time;
		std::chrono::steady_clock::time_point last_update;
		float progress_percentage;
		std::string error_message;
        
		// State data for migration
		std::vector<uint8_t> state_data;
		size_t state_size;
	};
    
	class SeamlessMigration
	{
	public:
		SeamlessMigration();
		~SeamlessMigration();
        
		// Configuration
		struct Config
		{
			std::chrono::milliseconds state_sync_timeout{30000};  // 30 seconds
			std::chrono::milliseconds handover_timeout{5000};     // 5 seconds
			uint32_t max_concurrent_migrations = 10;
			bool enable_compression = true;
			bool verify_after_migration = true;
			uint32_t retry_attempts = 3;
		};
        
		auto configure(const Config& config) -> void;
        
		// Migration operations
		auto migrate_session(const std::string& session_id,
							const std::string& target_server)
			-> std::tuple<bool, std::optional<std::string>>;
        
		auto migrate_sessions_batch(const std::vector<std::string>& session_ids, const std::string& target_server)
			-> std::tuple<bool, std::optional<std::string>>;
        
		auto migrate_server_load(const std::string& source_server, const std::string& target_server, float percentage = 50.0f)
			-> std::tuple<bool, std::optional<std::string>>;
        
		// Migration control
		auto pause_migration(const std::string& task_id) -> std::tuple<bool, std::optional<std::string>>;
		auto resume_migration(const std::string& task_id) -> std::tuple<bool, std::optional<std::string>>;
		auto cancel_migration(const std::string& task_id) -> std::tuple<bool, std::optional<std::string>>;
        
		// State management
		auto capture_session_state(const std::string& session_id) 
			-> std::tuple<bool, std::optional<std::string>, std::vector<uint8_t>>;
        
		auto restore_session_state(const std::string& session_id, const std::vector<uint8_t>& state_data)
			-> std::tuple<bool, std::optional<std::string>>;
        
		// Progress tracking
		auto get_migration_status(const std::string& task_id) const 
			-> std::optional<MigrationTask>;
        
		auto get_active_migrations() const -> std::vector<MigrationTask>;
        
		// Callbacks
		using MigrationCallback = std::function<void(const MigrationTask&)>;
		auto set_migration_started_callback(MigrationCallback callback) -> void;
		auto set_migration_completed_callback(MigrationCallback callback) -> void;
		auto set_migration_failed_callback(MigrationCallback callback) -> void;
        
		// Statistics
		struct MigrationStats
		{
			uint64_t total_migrations_attempted;
			uint64_t successful_migrations;
			uint64_t failed_migrations;
			std::chrono::milliseconds average_migration_time;
			uint64_t total_bytes_transferred;
			float success_rate;
		};
        
		auto get_statistics() const -> MigrationStats;
        
		// Migration initiation (internal)
		auto initiate_migration(const std::string& session_id, const std::string& target_server)
			-> std::tuple<bool, std::string>;
        
	private:
		auto execute_migration(MigrationTask& task) -> void;
		auto update_task_phase(MigrationTask& task, MigrationPhase phase) -> void;
		auto perform_state_snapshot(MigrationTask& task) -> std::tuple<bool, std::optional<std::string>>;
		auto perform_state_transfer(MigrationTask& task) -> std::tuple<bool, std::optional<std::string>>;
		auto perform_connection_handover(MigrationTask& task) -> std::tuple<bool, std::optional<std::string>>;
		auto verify_migration(MigrationTask& task) -> std::tuple<bool, std::optional<std::string>>;
        
	private:
		mutable std::mutex mutex_;
        
		// Configuration
		Config config_;
        
		// Task management
		std::unordered_map<std::string, MigrationTask> active_tasks_;
		std::atomic<uint32_t> task_counter_;
        
		// Dependencies
		std::weak_ptr<LoadBalancer> load_balancer_;
        
		// Callbacks
		MigrationCallback started_callback_;
		MigrationCallback completed_callback_;
		MigrationCallback failed_callback_;
        
		// Statistics
		MigrationStats stats_;
	};
}
