#include "PacketProcessor.h"
namespace GameNetwork { 
    PacketProcessor::PacketProcessor(std::shared_ptr<Thread::ThreadPool> thread_pool)
        : thread_pool_(thread_pool) {}
    PacketProcessor::~PacketProcessor() = default;
}
