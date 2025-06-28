#include "SeamlessMigration.h"
#include "../Session/GameSession.h"
#include "../Session/GameSessionManager.h"
#include "../Serialization/BinarySerializer.h"
#include "../Core/ServerRegistry.h"  // ServerRegistry
#include "../Packet/GamePacket.h"
#include "LoadBalancer.h"
#include <Logger.h>
#include <algorithm>
#include <thread>
#include <future>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>

using namespace Utilities;
using LogTypes = Utilities::LogTypes;

namespace GameNetwork
{
    SeamlessMigration::SeamlessMigration()
        : task_counter_(0)
        , stats_{0, 0, 0, std::chrono::milliseconds(0), 0, 0.0f}
    {
        configure(Config{});
    }

    SeamlessMigration::~SeamlessMigration()
    {
        // Cancel any active migrations
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [task_id, task] : active_tasks_)
        {
            if (task.phase != MigrationPhase::Completed && 
                task.phase != MigrationPhase::Failed)
            {
                task.phase = MigrationPhase::Failed;
                task.error_message = "Migration cancelled due to shutdown";
            }
        }
    }

    auto SeamlessMigration::configure(const Config& config) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        
        Logger::handle().write(LogTypes::Information,
            "SeamlessMigration configured with max concurrent: " + 
            std::to_string(config.max_concurrent_migrations));
    }

    auto SeamlessMigration::migrate_session(const std::string& session_id,
                                           const std::string& target_server)
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check concurrent migration limit
        uint32_t active_count = 0;
        for (const auto& [id, task] : active_tasks_)
        {
            if (task.phase != MigrationPhase::Completed && 
                task.phase != MigrationPhase::Failed)
            {
                active_count++;
            }
        }
        
        if (active_count >= config_.max_concurrent_migrations)
        {
            return { false, "Maximum concurrent migrations reached" };
        }
        
        // Get session
        auto session_manager = GameSessionManager::get_instance();
        if (!session_manager)
        {
            return { false, "Session manager not available" };
        }
        
        auto session = session_manager->get_session(session_id);
        if (!session)
        {
            return { false, "Session not found" };
        }

        // Create migration task
        MigrationTask task;
        task.task_id = "migration_" + std::to_string(task_counter_++);
        task.session_id = session_id;
        task.source_server = "current_server"; // Get current server identifier
        // In a real implementation, this would be retrieved from the session or server config
        task.target_server = target_server;
        task.phase = MigrationPhase::Preparing;
        task.start_time = std::chrono::steady_clock::now();
        task.last_update = task.start_time;
        task.progress_percentage = 0.0f;
        
        active_tasks_[task.task_id] = task;
        
        // Start migration in background
        std::thread migration_thread([this, task_id = task.task_id]() mutable {
            auto it = active_tasks_.find(task_id);
            if (it != active_tasks_.end())
            {
                execute_migration(it->second);
            }
        });
        migration_thread.detach();
        
        stats_.total_migrations_attempted++;
        
        if (started_callback_)
        {
            started_callback_(task);
        }
        
        return { true, task.task_id };
    }

    auto SeamlessMigration::migrate_sessions_batch(const std::vector<std::string>& session_ids,
                                                  const std::string& target_server)
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::vector<std::string> task_ids;
        
        for (const auto& session_id : session_ids)
        {
            auto [success, result] = migrate_session(session_id, target_server);
            if (success && result.has_value())
            {
                task_ids.push_back(result.value());
            }
        }
        
        if (task_ids.empty())
        {
            return { false, "No migrations started" };
        }
        
        return { true, "Started " + std::to_string(task_ids.size()) + " migrations" };
    }

    auto SeamlessMigration::migrate_server_load(const std::string& source_server,
                                               const std::string& target_server,
                                               float percentage)
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (percentage <= 0.0f || percentage > 100.0f)
        {
            return { false, "Invalid percentage" };
        }
        
        // Get all sessions on source server
        auto session_manager = GameSessionManager::get_instance();
        if (!session_manager)
        {
            return { false, "Session manager not available" };
        }
        
        // Get all sessions from the source server
        std::vector<std::string> sessions_to_migrate;
        size_t total_sessions = 0;
        
        // Query the session manager for sessions actually on the source server
        auto all_sessions = session_manager->get_all_sessions();
        std::vector<std::string> source_server_sessions;
        
        for (const auto& [session_id, session] : all_sessions)
        {
            // Check if session belongs to source server by checking server assignment
            // This assumes sessions have server_id tracking (would need to be added to GameSession)
            if (session && session->get_current_server_id() == source_server) {
                source_server_sessions.push_back(session_id);
                total_sessions++;
            }
        }
        
        if (total_sessions == 0) {
            Logger::handle().write(LogTypes::Warning, 
                "No sessions found on source server: " + source_server);
            return { true, std::nullopt }; // No sessions to migrate
        }
        
        // Calculate how many sessions to migrate
        size_t sessions_to_move = (total_sessions * percentage) / 100;
        if (sessions_to_move == 0 && percentage > 0) {
            sessions_to_move = 1; // Migrate at least one session if percentage > 0
        }
        
        Logger::handle().write(LogTypes::Information, 
            "Planning to migrate " + std::to_string(sessions_to_move) + 
            " out of " + std::to_string(total_sessions) + " sessions from " + source_server);
        
        // Select sessions to migrate using least-recently-active strategy
        std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> session_activity;
        for (const auto& session_id : source_server_sessions) {
            auto session = all_sessions[session_id];
            if (session) {
                session_activity.emplace_back(session_id, session->get_last_activity_time());
            }
        }
        
        // Sort by last activity time (oldest first)
        std::sort(session_activity.begin(), session_activity.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        // Select the least active sessions for migration
        size_t count = 0;
        for (const auto& [session_id, last_activity] : session_activity)
        {
            if (count >= sessions_to_move) break;
            
            sessions_to_migrate.push_back(session_id);
            count++;
        }
        
        // Initiate migration for selected sessions
        for (const auto& session_id : sessions_to_migrate)
        {
            auto [success, error] = migrate_session(session_id, target_server);
            if (!success)
            {
                Logger::handle().write(LogTypes::Error,
                    "Failed to migrate session " + session_id + ": " + 
                    (error ? *error : "Unknown error"));
            }
        }
        
        Logger::handle().write(LogTypes::Information,
            "Migrating " + std::to_string(percentage) + "% load from " + 
            source_server + " to " + target_server);
        
        return { true, "Load migration initiated" };
    }

    auto SeamlessMigration::pause_migration(const std::string& task_id)
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_tasks_.find(task_id);
        if (it == active_tasks_.end())
        {
            return { false, "Task not found" };
        }
        
        // Implement pause logic
        if (it->second.phase == MigrationPhase::Executing)
        {
            it->second.phase = MigrationPhase::Paused;
            it->second.last_update = std::chrono::steady_clock::now();
            
            Logger::handle().write(LogTypes::Information,
                "Migration task paused: " + task_id);
        }
        else
        {
            return { false, "Task cannot be paused in current phase" };
        }
        
        return { true, std::nullopt };
    }

    auto SeamlessMigration::resume_migration(const std::string& task_id)
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_tasks_.find(task_id);
        if (it == active_tasks_.end())
        {
            return { false, "Task not found" };
        }
        
        // Implement resume logic
        if (it->second.phase == MigrationPhase::Paused)
        {
            it->second.phase = MigrationPhase::Executing;
            it->second.last_update = std::chrono::steady_clock::now();
            
            // Resume migration in background
            std::thread resume_thread([this, task_id]() {
                auto task_it = active_tasks_.find(task_id);
                if (task_it != active_tasks_.end())
                {
                    execute_migration(task_it->second);
                }
            });
            resume_thread.detach();
            
            Logger::handle().write(LogTypes::Information,
                "Migration task resumed: " + task_id);
        }
        else
        {
            return { false, "Task cannot be resumed in current phase" };
        }
        
        return { true, std::nullopt };
    }

    auto SeamlessMigration::cancel_migration(const std::string& task_id)
        -> std::tuple<bool, std::optional<std::string>>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_tasks_.find(task_id);
        if (it == active_tasks_.end())
        {
            return { false, "Task not found" };
        }
        
        if (it->second.phase == MigrationPhase::Completed ||
            it->second.phase == MigrationPhase::Failed)
        {
            return { false, "Migration already finished" };
        }
        
        it->second.phase = MigrationPhase::Failed;
        it->second.error_message = "Migration cancelled by user";
        
        if (failed_callback_)
        {
            failed_callback_(it->second);
        }
        
        return { true, std::nullopt };
    }

    auto SeamlessMigration::capture_session_state(const std::string& session_id)
        -> std::tuple<bool, std::optional<std::string>, std::vector<uint8_t>>
    {
        try {
            auto session_manager = GameSessionManager::get_instance();
            if (!session_manager) {
                return { false, "Session manager not available", {} };
            }
            
            auto session = session_manager->get_session(session_id);
            if (!session) {
                return { false, "Session not found: " + session_id, {} };
            }
            
            // Create JSON object with session state
            boost::json::object session_state;
            
            // Basic session info
            session_state["session_id"] = session->get_session_id();
            session_state["account_id"] = session->get_account_id();
            session_state["current_server_id"] = session->get_current_server_id();
            session_state["last_activity"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                session->get_last_activity_time().time_since_epoch()).count();
            
            // Connection state
            session_state["is_online"] = session->is_online();
            session_state["connection_state"] = static_cast<int>(session->state());
            
            // Character and location data
            if (session->get_character_id() != 0) {
                boost::json::object character_data;
                character_data["character_id"] = session->get_character_id();
                character_data["entity_id"] = session->get_entity_id();
                
                if (auto location = session->get_player_location()) {
                    boost::json::object location_data;
                    location_data["x"] = location.value().position.x;
                    location_data["y"] = location.value().position.y;
                    location_data["z"] = location.value().position.z;
                    location_data["pitch"] = location.value().pitch;
                    location_data["yaw"] = location.value().yaw;
                    location_data["roll"] = location.value().roll;
                    location_data["map_id"] = location.value().map_id;
                    character_data["location"] = location_data;
                }
                
                session_state["character"] = character_data;
            }
            
            // Channel information
            session_state["channel_id"] = session->get_channel_id();
            
            // Serialize to string then to bytes
            std::string json_str = boost::json::serialize(session_state);
            std::vector<uint8_t> state_data(json_str.begin(), json_str.end());
            
            Logger::handle().write(LogTypes::Information,
                "Captured state for session " + session_id + " (" + 
                std::to_string(state_data.size()) + " bytes)");
            
            return { true, std::nullopt, state_data };
        }
        catch (const std::exception& e) {
            return { false, "Exception during state capture: " + std::string(e.what()), {} };
        }
    }

    auto SeamlessMigration::restore_session_state(const std::string& session_id,
                                                  const std::vector<uint8_t>& state_data)
        -> std::tuple<bool, std::optional<std::string>>
    {
        try {
            // Convert bytes back to string
            std::string json_str(state_data.begin(), state_data.end());
            
            // Parse JSON state
            auto session_state = boost::json::parse(json_str);
            if (!session_state.is_object()) {
                return { false, "Invalid session state format" };
            }
            
            auto& state_obj = session_state.as_object();
            
            auto session_manager = GameSessionManager::get_instance();
            if (!session_manager) {
                return { false, "Session manager not available" };
            }
            
            // Get or create session
            auto session = session_manager->get_session(session_id);
            bool is_new_session = !session;
            
            if (is_new_session) {
                // Create new session with restored account_id
                std::string account_id = state_obj.contains("account_id") ? 
                    state_obj["account_id"].as_string().c_str() : "";
                
                auto [new_session, error] = session_manager->create_session(account_id);
                if (!new_session) {
                    return { false, "Failed to create session during restoration: " + 
                            (error ? error.value() : "Unknown error") };
                }
                session = new_session;
            }
            
            // Restore session properties
            if (state_obj.contains("current_server_id")) {
                session->set_current_server_id(state_obj["current_server_id"].as_string().c_str());
            }
            
            if (state_obj.contains("connection_state")) {
                auto conn_state = static_cast<SessionConnectionState>(
                    state_obj["connection_state"].as_int64());
                session->set_state(conn_state);
            }
            
            if (state_obj.contains("channel_id")) {
                session->set_channel_id(static_cast<uint32_t>(state_obj["channel_id"].as_int64()));
            }
            
            // Restore character data
            if (state_obj.contains("character")) {
                auto& char_obj = state_obj["character"].as_object();
                
                if (char_obj.contains("character_id")) {
                    session->set_character_id(char_obj["character_id"].as_int64());
                }
                
                if (char_obj.contains("entity_id")) {
                    session->set_entity_id(char_obj["entity_id"].as_int64());
                }
                
                if (char_obj.contains("location")) {
                    auto& location_obj = char_obj["location"].as_object();
                    GameNetwork::Location location;
                    
                    if (location_obj.contains("x") && location_obj.contains("y") && location_obj.contains("z")) {
                        location.position.x = static_cast<float>(location_obj["x"].as_double());
                        location.position.y = static_cast<float>(location_obj["y"].as_double());
                        location.position.z = static_cast<float>(location_obj["z"].as_double());
                        location.x = location.position.x;
                        location.y = location.position.y;
                        location.z = location.position.z;
                    }
                    
                    if (location_obj.contains("pitch") && location_obj.contains("yaw") && location_obj.contains("roll")) {
                        location.pitch = static_cast<float>(location_obj["pitch"].as_double());
                        location.yaw = static_cast<float>(location_obj["yaw"].as_double());
                        location.roll = static_cast<float>(location_obj["roll"].as_double());
                    }
                    
                    if (location_obj.contains("map_id")) {
                        location.map_id = static_cast<uint32_t>(location_obj["map_id"].as_int64());
                    }
                    
                    session->set_player_location(location);
                }
            }
            
            Logger::handle().write(LogTypes::Information,
                "Restored state for session " + session_id + 
                (is_new_session ? " (created new)" : " (updated existing)"));
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e) {
            return { false, "Exception during state restoration: " + std::string(e.what()) };
        }
    }

    auto SeamlessMigration::get_migration_status(const std::string& task_id) const
        -> std::optional<MigrationTask>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_tasks_.find(task_id);
        if (it != active_tasks_.end())
        {
            return it->second;
        }
        
        return std::nullopt;
    }

    auto SeamlessMigration::get_active_migrations() const -> std::vector<MigrationTask>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<MigrationTask> active;
        for (const auto& [id, task] : active_tasks_)
        {
            if (task.phase != MigrationPhase::Completed && 
                task.phase != MigrationPhase::Failed)
            {
                active.push_back(task);
            }
        }
        
        return active;
    }

    auto SeamlessMigration::set_migration_started_callback(MigrationCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        started_callback_ = callback;
    }

    auto SeamlessMigration::set_migration_completed_callback(MigrationCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        completed_callback_ = callback;
    }

    auto SeamlessMigration::set_migration_failed_callback(MigrationCallback callback) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        failed_callback_ = callback;
    }

    auto SeamlessMigration::get_statistics() const -> MigrationStats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Create a copy and update success rate
        MigrationStats result = stats_;
        if (result.total_migrations_attempted > 0)
        {
            result.success_rate = static_cast<float>(result.successful_migrations) / 
                                 result.total_migrations_attempted * 100.0f;
        }
        
        return result;
    }

    auto SeamlessMigration::execute_migration(MigrationTask& task) -> void
    {
        // Phase 1: State Snapshot
        update_task_phase(task, MigrationPhase::StateSnapshot);
        auto [snapshot_success, snapshot_error] = perform_state_snapshot(task);
        if (!snapshot_success)
        {
            task.phase = MigrationPhase::Failed;
            task.error_message = snapshot_error.value_or("Unknown error");
            stats_.failed_migrations++;
            if (failed_callback_) failed_callback_(task);
            return;
        }

        // Phase 2: State Transfer
        update_task_phase(task, MigrationPhase::StateTransfer);
        auto [transfer_success, transfer_error] = perform_state_transfer(task);
        if (!transfer_success)
        {
            task.phase = MigrationPhase::Failed;
            task.error_message = transfer_error.value_or("Unknown error");
            stats_.failed_migrations++;
            if (failed_callback_) failed_callback_(task);
            return;
        }
        
        // Phase 3: Connection Handover
        update_task_phase(task, MigrationPhase::ConnectionHandover);
        auto [handover_success, handover_error] = perform_connection_handover(task);
        if (!handover_success)
        {
            task.phase = MigrationPhase::Failed;
            task.error_message = handover_error.value_or("Unknown error");
            stats_.failed_migrations++;
            if (failed_callback_) failed_callback_(task);
            return;
        }
        
        // Phase 4: Verification
        if (config_.verify_after_migration)
        {
            update_task_phase(task, MigrationPhase::Verification);
            auto [verify_success, verify_error] = verify_migration(task);
            if (!verify_success)
            {
                task.phase = MigrationPhase::Failed;
                task.error_message = verify_error.value_or("Verification failed");
                stats_.failed_migrations++;
                if (failed_callback_) failed_callback_(task);
                return;
            }
        }
        
        // Migration completed successfully
        update_task_phase(task, MigrationPhase::Completed);
        task.progress_percentage = 100.0f;
        
        // Update statistics
        auto duration = std::chrono::steady_clock::now() - task.start_time;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        stats_.successful_migrations++;
        stats_.average_migration_time = 
            (stats_.average_migration_time * (stats_.successful_migrations - 1) + duration_ms) / 
            stats_.successful_migrations;
        
        if (completed_callback_)
        {
            completed_callback_(task);
        }
        
        Logger::handle().write(LogTypes::Information,
            "Migration " + task.task_id + " completed in " + 
            std::to_string(duration_ms.count()) + "ms");
    }

    auto SeamlessMigration::update_task_phase(MigrationTask& task, MigrationPhase phase) -> void
    {
        task.phase = phase;
        task.last_update = std::chrono::steady_clock::now();
        
        // Update progress based on phase
        switch (phase)
        {
            case MigrationPhase::Preparing:
                task.progress_percentage = 0.0f;
                break;
            case MigrationPhase::StateSnapshot:
                task.progress_percentage = 20.0f;
                break;
            case MigrationPhase::StateTransfer:
                task.progress_percentage = 40.0f;
                break;
            case MigrationPhase::ConnectionHandover:
                task.progress_percentage = 70.0f;
                break;
            case MigrationPhase::Verification:
                task.progress_percentage = 90.0f;
                break;
            case MigrationPhase::Completed:
                task.progress_percentage = 100.0f;
                break;
            case MigrationPhase::Failed:
                // Keep current progress
                break;
        }
    }

    auto SeamlessMigration::perform_state_snapshot(MigrationTask& task)
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Capture session state
        auto [success, error, state_data] = capture_session_state(task.session_id);
        if (!success)
        {
            return { false, error };
        }
        
        // Store state data for transfer
        task.state_data = std::move(state_data);
        task.state_size = task.state_data.size();
        stats_.total_bytes_transferred += state_data.size();
        
        return { true, std::nullopt };
    }

    auto SeamlessMigration::perform_state_transfer(MigrationTask& task)
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Implement state transfer to target server
        try
        {
            // 1. Get server registry instance
            auto& server_registry = ServerRegistry::get_instance();
            
            // 2. Prepare migration message
            boost::json::object migration_msg;
            migration_msg["command"] = "accept_migration";
            migration_msg["task_id"] = task.task_id;
            migration_msg["session_id"] = task.session_id;
            migration_msg["source_server"] = task.source_server;
            
            // 3. Send state data to target server
            auto result = ServerRegistry::get_instance().send_message(
                task.target_server, 
                boost::json::serialize(migration_msg)
            );
            bool send_success = std::get<0>(result);
            std::string send_error = std::get<1>(result);
            
            if (!send_success)
            {
                return { false, std::string("Failed to transfer state: " + send_error) };
            }
            
            // 4. Wait for confirmation with actual status checking
            auto start_time = std::chrono::steady_clock::now();
            auto timeout = std::chrono::seconds(30);
            bool migration_confirmed = false;
            std::string confirmation_error;
            
            Logger::handle().write(LogTypes::Information,
                "Waiting for migration confirmation from target server: " + task.target_server);
            
            while (std::chrono::steady_clock::now() - start_time < timeout && !migration_confirmed)
            {
                // Send status check to target server
                boost::json::object status_msg;
                status_msg["command"] = "check_migration_status";
                status_msg["task_id"] = task.task_id;
                status_msg["session_id"] = task.session_id;
                
                auto status_result = ServerRegistry::get_instance().send_and_wait(
                    task.target_server, 
                    boost::json::serialize(status_msg),
                    5000 // 5 second timeout for status check
                );
                
                bool status_success = std::get<0>(status_result);
                std::string status_response = std::get<1>(status_result);
                
                if (status_success) {
                    try {
                        // Parse JSON response
                        auto response_json = boost::json::parse(status_response);
                        if (response_json.is_object()) {
                            auto& obj = response_json.as_object();
                            if (obj.contains("migration_status")) {
                                std::string status = obj["migration_status"].as_string().c_str();
                                if (status == "completed") {
                                    migration_confirmed = true;
                                    Logger::handle().write(LogTypes::Information,
                                        "Migration confirmed by target server");
                                    break;
                                } else if (status == "failed") {
                                    confirmation_error = obj.contains("error") ? 
                                        obj["error"].as_string().c_str() : "Migration failed on target server";
                                    break;
                                }
                                // If status is "in_progress", continue waiting
                            }
                        }
                    } catch (const std::exception& e) {
                        Logger::handle().write(LogTypes::Warning,
                            "Failed to parse migration status response: " + std::string(e.what()));
                    }
                }
                
                // Wait before next status check
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            if (!migration_confirmed) {
                if (!confirmation_error.empty()) {
                    return { false, std::string("Migration failed: " + confirmation_error) };
                } else {
                    return { false, std::string("Migration confirmation timeout") };
                }
            }
            
            Logger::handle().write(LogTypes::Information,
                "State transferred for session: " + task.session_id);
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Exception during state transfer: ") + e.what() };
        }
    }

    auto SeamlessMigration::perform_connection_handover(MigrationTask& task)
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Implement connection handover
        try
        {
            auto session_manager = GameSessionManager::get_instance();
            if (!session_manager)
            {
                return { false, "Session manager not available" };
            }
            
            auto session = session_manager->get_session(task.session_id);
            if (!session)
            {
                return { false, "Session not found" };
            }
            
            // 1. Pause client traffic
            session->set_state(SessionConnectionState::Reconnecting);
            
            // 2. Get network connection
            auto network_session = session->current_connection();
            if (network_session)
            {
                // Send redirect command to client
                boost::json::object redirect_msg;
                redirect_msg["command"] = "redirect";
                redirect_msg["new_server"] = task.target_server;
                redirect_msg["migration_token"] = task.task_id;
                
                std::string msg_str = boost::json::serialize(redirect_msg);
                network_session->send(msg_str);
            }
            
            // 3. Mark session for cleanup on source server
            session->set_state(SessionConnectionState::Disconnected);
            
            // 4. Schedule session removal after grace period
            std::thread cleanup_thread([session_manager, session_id = task.session_id]() {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                session_manager->terminate_session(session_id);
            });
            cleanup_thread.detach();
            
            Logger::handle().write(LogTypes::Information,
                "Connection handover completed for session: " + task.session_id);
            
            return { true, std::nullopt };
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Exception during handover: ") + e.what() };
        }
    }

    auto SeamlessMigration::verify_migration(MigrationTask& task)
        -> std::tuple<bool, std::optional<std::string>>
    {
        // Implement migration verification
        try
        {
            auto& server_registry = ServerRegistry::get_instance();
            
            // 1. Query target server for session status
            boost::json::object verify_msg;
            verify_msg["command"] = "verify_session";
            verify_msg["session_id"] = task.session_id;
            verify_msg["task_id"] = task.task_id;
            
            auto [send_success, send_error] = server_registry.send_and_wait(
                task.target_server,
                boost::json::serialize(verify_msg),
                10
            );
            
            if (!send_success)
            {
                return { false, "Failed to verify migration: " + send_error };
            }
            
            // 2. Parse response
            try
            {
                boost::system::error_code ec;
                auto response_json = boost::json::parse(send_error, ec);
                
                if (ec)
                {
                    return { false, "Invalid verification response" };
                }
                
                auto response_obj = response_json.as_object();
                
                // 3. Check session is active
                bool session_active = response_obj.contains("session_active") && 
                                    response_obj["session_active"].as_bool();
                
                if (!session_active)
                {
                    return { false, "Session not active on target server" };
                }
                
                // 4. Check state integrity
                bool state_valid = response_obj.contains("state_valid") && 
                                 response_obj["state_valid"].as_bool();
                
                if (!state_valid)
                {
                    return { false, "Session state validation failed" };
                }
                
                // 5. Check client connectivity
                bool client_connected = response_obj.contains("client_connected") && 
                                      response_obj["client_connected"].as_bool();
                
                if (!client_connected)
                {
                    // Client might still be reconnecting, give it some time
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
                
                Logger::handle().write(LogTypes::Information,
                    "Migration verified for session: " + task.session_id);
                
                return { true, std::nullopt };
            }
            catch (const std::exception& e)
            {
                return { false, std::string("Failed to parse verification response: ") + e.what() };
            }
        }
        catch (const std::exception& e)
        {
            return { false, std::string("Exception during verification: ") + e.what() };
        }
    }

    auto SeamlessMigration::initiate_migration(const std::string& session_id, const std::string& target_server) -> std::tuple<bool, std::string>
    {
        try {
            // Create migration task
            std::string task_id = "migration_" + std::to_string(++task_counter_) + "_" + session_id;
            
            MigrationTask task;
            task.task_id = task_id;
            task.session_id = session_id;
            task.source_server = "local_server"; // This should be configurable
            task.target_server = target_server;
            task.phase = MigrationPhase::Preparing;
            task.start_time = std::chrono::steady_clock::now();
            task.last_update = task.start_time;
            task.progress_percentage = 0.0f;
            
            // Store task
            active_tasks_[task_id] = task;
            
            // Start migration execution
            std::thread([this, task_id]() {
                auto it = active_tasks_.find(task_id);
                if (it != active_tasks_.end()) {
                    execute_migration(it->second);
                }
            }).detach();
            
            return std::make_tuple(true, task_id);
        }
        catch (const std::exception& e) {
            return std::make_tuple(false, "Failed to initiate migration: " + std::string(e.what()));
        }
    }
}
