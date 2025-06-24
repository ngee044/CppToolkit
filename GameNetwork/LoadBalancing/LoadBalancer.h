#pragma once

#include <string>
#include <memory>

namespace GameNetwork
{
    class LoadBalancer
    {
    public:
        LoadBalancer();
        ~LoadBalancer();
        
        auto set_server_id(const std::string& server_id) -> void;
        
    private:
        std::string server_id_;
    };
}
