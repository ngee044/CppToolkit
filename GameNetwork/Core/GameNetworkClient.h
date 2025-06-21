#pragma once

#include "../GameNetworkConstants.h"
#include "../../Network/NetworkClient.h"

namespace GameNetwork
{
    class GameNetworkClient
    {
    public:
        GameNetworkClient();
        virtual ~GameNetworkClient() = default;
        
        // TODO: Define client interface
    };
}
