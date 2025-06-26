#include "HealthCheck.h"
#include <Logger.h>
#include <NetworkClient.h>
#include <chrono>
#include <curl/curl.h>

namespace GameNetwork
{
    namespace LoadBalancing
    {
        // TCP Health Checker Implementation
        TcpHealthChecker::TcpHealthChecker(const std::string& host, uint16_t port, 
                                           std::chrono::seconds timeout)
            : host_(host), port_(port), timeout_(timeout)
        {
        }
        
        auto TcpHealthChecker::check_health() -> std::tuple<bool, HealthCheckResult>
        {
            HealthCheckResult result;
            result.timestamp = std::chrono::steady_clock::now();
            
            auto start_time = std::chrono::steady_clock::now();
            
            // Try to establish TCP connection
            Network::NetworkClient client;
            client.set_connection_timeout(timeout_);
            
            auto connect_result = client.connect(host_, port_);
            
            auto end_time = std::chrono::steady_clock::now();
            result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (std::get<0>(connect_result))
            {
                result.status = HealthStatus::Healthy;
                result.details = "TCP connection successful";
                client.disconnect();
                return {true, result};
            }
            else
            {
                result.status = HealthStatus::Unhealthy;
                result.details = "TCP connection failed: " + std::get<1>(connect_result).value_or("Unknown error");
                return {false, result};
            }
        }
        
        // HTTP Health Checker Implementation  
        HttpHealthChecker::HttpHealthChecker(const std::string& url, std::chrono::seconds timeout)
            : url_(url), timeout_(timeout)
        {
        }
        
        auto HttpHealthChecker::check_health() -> std::tuple<bool, HealthCheckResult>
        {
            HealthCheckResult result;
            result.timestamp = std::chrono::steady_clock::now();
            
#ifdef HAVE_CURL
            CURL* curl = curl_easy_init();
            if (!curl)
            {
                result.status = HealthStatus::Unknown;
                result.details = "Failed to initialize CURL";
                return {false, result};
            }
            
            auto start_time = std::chrono::steady_clock::now();
            
            // Set CURL options
            curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_.count());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
            
            // Perform request
            CURLcode res = curl_easy_perform(curl);
            
            auto end_time = std::chrono::steady_clock::now();
            result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (res == CURLE_OK)
            {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                
                if (response_code >= 200 && response_code < 300)
                {
                    result.status = HealthStatus::Healthy;
                    result.details = "HTTP " + std::to_string(response_code) + " OK";
                }
                else
                {
                    result.status = HealthStatus::Unhealthy;
                    result.details = "HTTP " + std::to_string(response_code);
                }
            }
            else
            {
                result.status = HealthStatus::Unhealthy;
                result.details = "HTTP request failed: " + std::string(curl_easy_strerror(res));
            }
            
            curl_easy_cleanup(curl);
            return {result.status == HealthStatus::Healthy, result};
#else
            result.status = HealthStatus::Unknown;
            result.details = "HTTP health check not available (CURL not enabled)";
            return {false, result};
#endif
        }
        
        // Custom Health Checker Implementation
        CustomHealthChecker::CustomHealthChecker(CheckFunction check_func, const std::string& name)
            : check_func_(check_func), name_(name)
        {
        }
        
        auto CustomHealthChecker::check_health() -> std::tuple<bool, HealthCheckResult>
        {
            if (check_func_)
            {
                return check_func_();
            }
            
            HealthCheckResult result;
            result.status = HealthStatus::Unknown;
            result.details = "No check function provided";
            result.timestamp = std::chrono::steady_clock::now();
            return {false, result};
        }
        
        // Server Health Monitor Implementation
        ServerHealthMonitor::ServerHealthMonitor(const std::string& server_id, 
                                                 const HealthCheckConfig& config)
            : server_id_(server_id), config_(config)
        {
        }
        
        ServerHealthMonitor::~ServerHealthMonitor()
        {
            stop();
        }
        
        auto ServerHealthMonitor::start() -> void
        {
            if (is_running_)
                return;
                
            is_running_ = true;
            should_stop_ = false;
            
            monitor_thread_ = std::thread([this]() { monitor_loop(); });
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Health monitor started for server: " + server_id_);
        }
        
        auto ServerHealthMonitor::stop() -> void
        {
            if (!is_running_)
                return;
                
            should_stop_ = true;
            
            if (monitor_thread_.joinable())
            {
                monitor_thread_.join();
            }
            
            is_running_ = false;
            
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "Health monitor stopped for server: " + server_id_);
        }
        
        auto ServerHealthMonitor::add_health_checker(std::unique_ptr<IHealthChecker> checker) -> void
        {
            health_checkers_.push_back(std::move(checker));
        }
        
        auto ServerHealthMonitor::get_last_check_result() const -> std::optional<HealthCheckResult>
        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            return last_result_;
        }
        
        auto ServerHealthMonitor::check_now() -> std::tuple<bool, HealthCheckResult>
        {
            auto result = perform_health_checks();
            update_status(result);
            return {result.status == HealthStatus::Healthy, result};
        }
        
        auto ServerHealthMonitor::monitor_loop() -> void
        {
            while (!should_stop_)
            {
                auto result = perform_health_checks();
                update_status(result);
                
                // Sleep for check interval
                for (int i = 0; i < config_.check_interval.count() && !should_stop_; ++i)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        }
        
        auto ServerHealthMonitor::perform_health_checks() -> HealthCheckResult
        {
            HealthCheckResult combined_result;
            combined_result.timestamp = std::chrono::steady_clock::now();
            combined_result.status = HealthStatus::Healthy;
            combined_result.response_time = std::chrono::milliseconds(0);
            
            if (health_checkers_.empty())
            {
                combined_result.status = HealthStatus::Unknown;
                combined_result.details = "No health checkers configured";
                return combined_result;
            }
            
            std::vector<std::string> check_details;
            int failed_checks = 0;
            
            for (const auto& checker : health_checkers_)
            {
                auto [success, result] = checker->check_health();
                
                // Update combined response time (use max)
                if (result.response_time > combined_result.response_time)
                {
                    combined_result.response_time = result.response_time;
                }
                
                // Aggregate metrics
                combined_result.cpu_usage = std::max(combined_result.cpu_usage, result.cpu_usage);
                combined_result.memory_usage = std::max(combined_result.memory_usage, result.memory_usage);
                combined_result.active_connections += result.active_connections;
                combined_result.request_rate += result.request_rate;
                
                check_details.push_back(checker->get_check_type() + ": " + result.details);
                
                if (!success)
                {
                    failed_checks++;
                }
            }
            
            // Determine overall status
            if (failed_checks == 0)
            {
                combined_result.status = HealthStatus::Healthy;
            }
            else if (failed_checks < health_checkers_.size())
            {
                combined_result.status = HealthStatus::Degraded;
            }
            else
            {
                combined_result.status = HealthStatus::Unhealthy;
            }
            
            // Combine details
            combined_result.details = std::to_string(health_checkers_.size() - failed_checks) + 
                                      "/" + std::to_string(health_checkers_.size()) + " checks passed";
            
            return combined_result;
        }
        
        auto ServerHealthMonitor::update_status(const HealthCheckResult& result) -> void
        {
            {
                std::lock_guard<std::mutex> lock(result_mutex_);
                last_result_ = result;
            }
            
            auto old_status = current_status_.load();
            auto new_status = result.status;
            
            // Update consecutive counters
            if (result.status == HealthStatus::Healthy)
            {
                consecutive_failures_ = 0;
                consecutive_successes_++;
                
                // Check if we should transition to healthy
                if (old_status != HealthStatus::Healthy && 
                    consecutive_successes_ >= config_.success_threshold)
                {
                    current_status_ = HealthStatus::Healthy;
                }
            }
            else if (result.status == HealthStatus::Unhealthy)
            {
                consecutive_successes_ = 0;
                consecutive_failures_++;
                
                // Check if we should transition to unhealthy
                if (old_status != HealthStatus::Unhealthy && 
                    consecutive_failures_ >= config_.failure_threshold)
                {
                    current_status_ = HealthStatus::Unhealthy;
                }
            }
            else if (result.status == HealthStatus::Degraded)
            {
                // Degraded doesn't reset counters but updates status immediately
                current_status_ = HealthStatus::Degraded;
            }
            
            // Notify if status changed
            if (old_status != current_status_ && status_change_callback_)
            {
                status_change_callback_(server_id_, old_status, current_status_);
            }
        }
        
    } // namespace LoadBalancing
} // namespace GameNetwork
