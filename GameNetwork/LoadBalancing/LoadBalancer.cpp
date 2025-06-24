#include "LoadBalancer.h"

namespace GameNetwork
{
    LoadBalancer::LoadBalancer() = default;
    LoadBalancer::~LoadBalancer() = default;
    
    auto LoadBalancer::set_server_id(const std::string& server_id) -> void
    {
        server_id_ = server_id;
    }
}
