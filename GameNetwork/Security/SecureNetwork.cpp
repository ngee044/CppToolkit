#include "SecureNetwork.h"
#include <Logger.h>

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#else
// Stub implementation when OpenSSL is not available
struct ssl_st {};
struct ssl_ctx_st {};
struct x509_st {};
#endif

namespace GameNetwork
{
    namespace Security
    {
        SSLContext::SSLContext()
            : ctx_(nullptr)
            , is_server_context_(true)
        {
#ifdef USE_OPENSSL
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
#endif
        }
        
        SSLContext::~SSLContext()
        {
#ifdef USE_OPENSSL
            if (ctx_)
            {
                SSL_CTX_free(ctx_);
            }
#endif
        }
        
        auto SSLContext::initialize(const SSLConfig& config) -> std::tuple<bool, std::optional<std::string>>
        {
#ifdef USE_OPENSSL
            // Create SSL context
            const SSL_METHOD* method = TLS_server_method();
            ctx_ = SSL_CTX_new(method);
            
            if (!ctx_)
            {
                return {false, "Failed to create SSL context"};
            }
            
            // Set minimum TLS version to 1.2
            SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
            
            // Load certificate
            if (!config.certificate_file.empty())
            {
                if (SSL_CTX_use_certificate_file(ctx_, config.certificate_file.c_str(), SSL_FILETYPE_PEM) <= 0)
                {
                    return {false, "Failed to load certificate file"};
                }
            }
            
            // Load private key
            if (!config.private_key_file.empty())
            {
                if (SSL_CTX_use_PrivateKey_file(ctx_, config.private_key_file.c_str(), SSL_FILETYPE_PEM) <= 0)
                {
                    return {false, "Failed to load private key"};
                }
                
                // Verify private key
                if (!SSL_CTX_check_private_key(ctx_))
                {
                    return {false, "Private key does not match certificate"};
                }
            }
            
            // Set cipher list
            if (!config.cipher_list.empty())
            {
                if (SSL_CTX_set_cipher_list(ctx_, config.cipher_list.c_str()) != 1)
                {
                    return {false, "Failed to set cipher list"};
                }
            }
            
            // Configure client verification
            if (config.verify_client && !config.ca_file.empty())
            {
                if (SSL_CTX_load_verify_locations(ctx_, config.ca_file.c_str(), nullptr) != 1)
                {
                    return {false, "Failed to load CA file"};
                }
                
                SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
                SSL_CTX_set_verify_depth(ctx_, config.verify_depth);
            }
            
            // Enable session cache
            if (config.use_session_cache)
            {
                SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);
                SSL_CTX_sess_set_new_cb(ctx_, new_session_callback);
                SSL_CTX_sess_set_remove_cb(ctx_, remove_session_callback);
            }
            
            return {true, std::nullopt};
#else
            return {false, "SSL support not compiled (OpenSSL required)"};
#endif
        }
