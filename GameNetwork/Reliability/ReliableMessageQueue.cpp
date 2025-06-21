#include "ReliableMessageQueue.h"
namespace GameNetwork { 
    ReliableMessageQueue::ReliableMessageQueue() 
        : global_sequence_(0)
        , max_retries_(3)
        , base_retry_delay_(std::chrono::milliseconds(100))
        , backoff_multiplier_(2.0f)
        , connection_active_(true) {}
    ReliableMessageQueue::~ReliableMessageQueue() = default;
}
