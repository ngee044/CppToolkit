#include "GameNetworkServer.h"
#include "../Monitoring/SystemMonitor.h"
#include "../Session/DisconnectionHandler.h"

namespace GameNetwork
{
    auto GameNetworkServer::get_system_monitor() -> std::shared_ptr<Monitoring::SystemMonitor>
    {
        return system_monitor_;
    }
    
    auto GameNetworkServer::get_disconnection_handler() -> std::shared_ptr<DisconnectionHandler>
    {
        return disconnection_handler_;
    }
    
    auto GameNetworkServer::start_monitoring() -> void
    {
        if (is_monitoring_.exchange(true))
        {
            return;
        }
        
        if (system_monitor_)
        {
            system_monitor_->start_recording(std::chrono::seconds(5));
            
            // Log monitoring started
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "System monitoring started");
        }
    }    auto GameNetworkServer::stop_monitoring() -> void
    {
        if (!is_monitoring_.exchange(false))
        {
            return;
        }
        
        if (system_monitor_)
        {
            system_monitor_->stop_recording();
            
            // Log monitoring stopped
            Utilities::Logger::handle().write(Utilities::LogTypes::Information,
                "System monitoring stopped");
        }
    }
    
} // namespace GameNetwork