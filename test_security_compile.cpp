// Test compilation of SessionSecurityManager
#include "GameNetwork/Session/Security/SessionSecurityManager.h"
#include <iostream>

int main()
{
    GameNetwork::Security::SessionSecurityPolicy policy;
    policy.max_concurrent_sessions_per_account = 2;
    policy.enable_session_hijacking_protection = true;
    
    GameNetwork::Security::SessionSecurityManager security_manager;
    auto [success, error] = security_manager.initialize(policy);
    
    if (success)
    {
        std::cout << "SessionSecurityManager initialized successfully\n";
    }
    else
    {
        std::cout << "Failed to initialize: " << error.value_or("Unknown error") << "\n";
    }
    
    return 0;
}