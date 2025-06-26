#pragma once

#include <string>
#include <memory>
#include <optional>
#include <tuple>
#include <functional>

// Forward declarations for OpenSSL types
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_st X509;

namespace GameNetwork
{
    namespace Security
    {
        // SSL/TLS configuration
        struct SSLConfig
        {
            std::string certificate_file;      // Server certificate
            std::string private_key_file;      // Private key
            std::string ca_file;               // CA certificate for client verification
            std::string cipher_list;           // Allowed ciphers
            bool verify_client;                // Require client certificate
            bool use_session_cache;            // Enable session resumption
            int verify_depth;                  // Certificate chain verification depth
            
            // Default secure configuration
            SSLConfig()
                : cipher_list("TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:ECDHE-RSA-AES256-GCM-SHA384")
                , verify_client(false)
                , use_session_cache(true)
                , verify_depth(3)
            {}
        };
        
        // SSL/TLS context wrapper
        class SSLContext
        {
        public:
            SSLContext();
            ~SSLContext();
            
            // Initialize SSL context
            auto initialize(const SSLConfig& config) -> std::tuple<bool, std::optional<std::string>>;
            
            // Create SSL connection
            auto create_ssl_connection(int socket_fd) -> std::tuple<SSL*, std::optional<std::string>>;
            
            // Server operations
            auto accept_ssl_connection(SSL* ssl) -> std::tuple<bool, std::optional<std::string>>;
            
            // Client operations
            auto connect_ssl(SSL* ssl) -> std::tuple<bool, std::optional<std::string>>;
            
            // Certificate verification
            auto verify_peer_certificate(SSL* ssl) -> std::tuple<bool, std::optional<std::string>>;
            
            // Get peer information
            auto get_peer_certificate_info(SSL* ssl) -> std::optional<std::string>;
            
            // Session management
            auto enable_session_resumption(bool enable) -> void;
            
            // Shutdown SSL connection
            auto shutdown_ssl(SSL* ssl) -> void;
            
        private:
            SSL_CTX* ctx_;
            bool is_server_context_;
            
            // Certificate callbacks
            static auto verify_callback(int preverify_ok, X509_STORE_CTX* ctx) -> int;
            
            // Session callbacks
            static auto new_session_callback(SSL* ssl, SSL_SESSION* session) -> int;
            static auto remove_session_callback(SSL_CTX* ctx, SSL_SESSION* session) -> void;
        };
        
        // Secure network server
        class SecureNetworkServer
        {
        public:
            SecureNetworkServer();
            ~SecureNetworkServer();
            
            // Enable SSL/TLS
            auto enable_ssl(const SSLConfig& config) -> std::tuple<bool, std::optional<std::string>>;
            
            // Check if SSL is enabled
            auto is_ssl_enabled() const -> bool { return ssl_enabled_; }
            
            // Accept secure connection
            auto accept_secure_connection(int client_socket) 
                -> std::tuple<bool, SSL*, std::optional<std::string>>;
            
            // Send/Receive secure data
            auto send_secure(SSL* ssl, const void* data, size_t size) 
                -> std::tuple<ssize_t, std::optional<std::string>>;
            auto receive_secure(SSL* ssl, void* buffer, size_t size) 
                -> std::tuple<ssize_t, std::optional<std::string>>;
            
            // Connection info
            auto get_cipher_info(SSL* ssl) -> std::string;
            auto get_protocol_version(SSL* ssl) -> std::string;
            
            // Callbacks
            using SSLErrorCallback = std::function<void(const std::string&)>;
            auto on_ssl_error(SSLErrorCallback callback) -> void { ssl_error_callback_ = callback; }
            
        private:
            std::unique_ptr<SSLContext> ssl_context_;
            bool ssl_enabled_;
            SSLConfig config_;
            SSLErrorCallback ssl_error_callback_;
            
            // Error handling
            auto get_ssl_error_string(SSL* ssl, int ret) -> std::string;
        };
        
        // Secure network client
        class SecureNetworkClient
        {
        public:
            SecureNetworkClient();
            ~SecureNetworkClient();
            
            // Enable SSL/TLS
            auto enable_ssl(const SSLConfig& config) -> std::tuple<bool, std::optional<std::string>>;
            
            // Connect with SSL
            auto connect_secure(int socket_fd, const std::string& hostname) 
                -> std::tuple<bool, SSL*, std::optional<std::string>>;
            
            // Verify server certificate
            auto verify_server_certificate(SSL* ssl, const std::string& expected_hostname) 
                -> std::tuple<bool, std::optional<std::string>>;
            
            // Send/Receive secure data
            auto send_secure(SSL* ssl, const void* data, size_t size) 
                -> std::tuple<ssize_t, std::optional<std::string>>;
            auto receive_secure(SSL* ssl, void* buffer, size_t size) 
                -> std::tuple<ssize_t, std::optional<std::string>>;
            
        private:
            std::unique_ptr<SSLContext> ssl_context_;
            bool ssl_enabled_;
            SSLConfig config_;
        };
        
    } // namespace Security
} // namespace GameNetwork
