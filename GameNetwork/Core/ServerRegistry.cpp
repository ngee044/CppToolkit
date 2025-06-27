#include "ServerRegistry.h"
#include "../Core/GameNetworkServer.h"
#include <Logger.h>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <curl/curl.h>

using namespace Utilities;

namespace GameNetwork {

// Helper struct for HTTP response
struct HttpResponse {
    std::string data;
    long response_code = 0;
    
    HttpResponse() = default;
};

// Callback function for libcurl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, HttpResponse* response) {
    size_t total_size = size * nmemb;
    response->data.append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Helper function to perform HTTP POST request
static HttpResponse perform_http_request(const std::string& url, const std::string& payload, int timeout_ms = 5000) {
    HttpResponse response;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.response_code = 0;
        response.data = "Failed to initialize curl";
        return response;
    }
    
    // Set up curl options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.length());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.response_code);
    } else {
        response.response_code = 0;
        response.data = "Request failed: " + std::string(curl_easy_strerror(res));
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return response;
}

ServerRegistry& ServerRegistry::get_instance()
{
    static ServerRegistry instance;
    return instance;
}

void ServerRegistry::register_local_server(std::shared_ptr<GameNetworkServer> server)
{
    local_server_ = server;
    Logger::handle().write(LogTypes::Information, "Local game server registered");
}

std::shared_ptr<GameNetworkServer> ServerRegistry::get_local_server()
{
    return local_server_;
}

void ServerRegistry::register_remote_server(const std::string& server_id, const std::string& endpoint)
{
    remote_servers_[server_id] = endpoint;
    Logger::handle().write(LogTypes::Information, "Remote server registered: " + server_id + " -> " + endpoint);
}

void ServerRegistry::unregister_remote_server(const std::string& server_id)
{
    auto it = remote_servers_.find(server_id);
    if (it != remote_servers_.end()) {
        remote_servers_.erase(it);
        Logger::handle().write(LogTypes::Information, "Remote server unregistered: " + server_id);
    }
}

std::vector<std::string> ServerRegistry::get_registered_servers() const
{
    std::vector<std::string> server_ids;
    server_ids.reserve(remote_servers_.size());
    
    for (const auto& [server_id, endpoint] : remote_servers_) {
        server_ids.push_back(server_id);
    }
    
    return server_ids;
}

bool ServerRegistry::is_server_registered(const std::string& server_id) const
{
    return remote_servers_.find(server_id) != remote_servers_.end();
}

std::string ServerRegistry::send_network_request(const std::string& url, const std::string& payload)
{
    try {
        Logger::handle().write(LogTypes::Information, "Sending HTTP request to: " + url);
        
        HttpResponse response = perform_http_request(url, payload);
        
        if (response.response_code >= 200 && response.response_code < 300) {
            Logger::handle().write(LogTypes::Information, "HTTP request successful, response code: " + std::to_string(response.response_code));
            return response.data;
        } else {
            std::string error_msg = "HTTP request failed with code: " + std::to_string(response.response_code) + ", response: " + response.data;
            Logger::handle().write(LogTypes::Error, error_msg);
            return R"({"status": "error", "message": ")" + error_msg + "\"}";
        }
    }
    catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Network request exception: " + std::string(e.what()));
        return R"({"status": "error", "message": "Request failed with exception"})";
    }
}

std::tuple<bool, std::string> ServerRegistry::send_message(const std::string& target_server, const std::string& message)
{
    try {
        // Check if target server is registered
        if (!is_server_registered(target_server)) {
            Logger::handle().write(LogTypes::Warning, "Target server not registered: " + target_server);
            return std::make_tuple(false, "Target server not registered: " + target_server);
        }
        
        // Get endpoint for target server
        auto endpoint = remote_servers_.at(target_server);
        
        // Construct full URL for inter-server messaging endpoint
        std::string url = endpoint + "/api/inter-server/message";
        
        // Create JSON payload
        std::string payload = R"({"message": ")" + message + R"(", "timestamp": ")" + 
                             std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}";
        
        Logger::handle().write(LogTypes::Information, "Sending message to server: " + target_server + " (" + endpoint + ")");
        
        HttpResponse response = perform_http_request(url, payload, 10000); // 10 second timeout
        
        if (response.response_code >= 200 && response.response_code < 300) {
            Logger::handle().write(LogTypes::Information, "Message sent successfully to " + target_server);
            return std::make_tuple(true, response.data.empty() ? "Message sent successfully" : response.data);
        } else {
            std::string error_msg = "Failed to send message, HTTP code: " + std::to_string(response.response_code) + ", response: " + response.data;
            Logger::handle().write(LogTypes::Error, error_msg);
            return std::make_tuple(false, error_msg);
        }
    }
    catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Failed to send message: " + std::string(e.what()));
        return std::make_tuple(false, "Failed to send message: " + std::string(e.what()));
    }
}

std::tuple<bool, std::string> ServerRegistry::send_and_wait(const std::string& target_server, const std::string& message, int timeout_ms)
{
    try {
        // Check if target server is registered
        if (!is_server_registered(target_server)) {
            Logger::handle().write(LogTypes::Warning, "Target server not registered: " + target_server);
            return std::make_tuple(false, "Target server not registered: " + target_server);
        }
        
        // Get endpoint for target server
        auto endpoint = remote_servers_.at(target_server);
        
        // Construct full URL for synchronous inter-server messaging endpoint
        std::string url = endpoint + "/api/inter-server/sync-message";
        
        // Create JSON payload with request ID for tracking
        std::string request_id = "req_" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        std::string payload = R"({"message": ")" + message + R"(", "request_id": ")" + request_id + 
                             R"(", "timestamp": ")" + 
                             std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}";
        
        Logger::handle().write(LogTypes::Information, 
            "Sending synchronous message to server: " + target_server + " (" + endpoint + ") with timeout: " + std::to_string(timeout_ms) + "ms");
        
        auto start_time = std::chrono::steady_clock::now();
        HttpResponse response = perform_http_request(url, payload, timeout_ms);
        auto end_time = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (response.response_code >= 200 && response.response_code < 300) {
            Logger::handle().write(LogTypes::Information, 
                "Synchronous message completed successfully to " + target_server + " in " + std::to_string(duration.count()) + "ms");
            return std::make_tuple(true, response.data.empty() ? R"({"status": "success", "response": "Operation completed"})" : response.data);
        } else if (response.response_code == 0) {
            std::string error_msg = "Request timeout or connection failed after " + std::to_string(duration.count()) + "ms: " + response.data;
            Logger::handle().write(LogTypes::Error, error_msg);
            return std::make_tuple(false, error_msg);
        } else {
            std::string error_msg = "Synchronous message failed, HTTP code: " + std::to_string(response.response_code) + ", response: " + response.data;
            Logger::handle().write(LogTypes::Error, error_msg);
            return std::make_tuple(false, error_msg);
        }
    }
    catch (const std::exception& e) {
        Logger::handle().write(LogTypes::Error, "Failed to send sync message: " + std::string(e.what()));
        return std::make_tuple(false, "Failed to send sync message: " + std::string(e.what()));
    }
}

} // namespace GameNetwork
