#include "MessageDispatcher.h"
#include "GameSession.h"

namespace GameNetwork { 
    MessageDispatcher::MessageDispatcher() 
        : rate_limiting_enabled_(false)
        , processing_running_(false) {}
        
    MessageDispatcher::~MessageDispatcher() = default;
    
    auto MessageDispatcher::register_handler(PacketType type, PacketHandler handler) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[type] = handler;
    }
    
    auto MessageDispatcher::unregister_handler(PacketType type) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(type);
    }
    
    auto MessageDispatcher::has_handler(PacketType type) const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.find(type) != handlers_.end();
    }
    
    auto MessageDispatcher::dispatch(std::shared_ptr<GameSession> session, 
                                   std::unique_ptr<GamePacket> packet) 
        -> std::tuple<bool, std::optional<std::string>>
    {
        if (!session || !packet)
        {
            return {false, "Invalid session or packet"};
        }
        
        PacketType type = packet->type();
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(type);
        if (it == handlers_.end())
        {
            return {false, "No handler registered for packet type"};
        }
        
        return it->second(session, *packet);
    }
    
    auto MessageDispatcher::register_default_handlers() -> void
    {
        // TODO: Register default packet handlers
    }
}
