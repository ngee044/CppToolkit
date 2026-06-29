#pragma once

#include <cstddef>

namespace Network
{
    constexpr size_t START_CODE_SIZE = 4;
    constexpr size_t LENGTH_SIZE = 8;
    constexpr size_t END_CODE_SIZE = 4;
    // Minimum receiving buffer size: must hold the largest single control read
    // (length/start/end code) so a small socket_buffer_size cannot overflow it.
    constexpr size_t MIN_RECEIVING_BUFFER_SIZE = LENGTH_SIZE;
    // Maximum allowed payload size per frame (after mode byte), in bytes
    // Protects against oversized frames exhausting memory. Adjust as needed.
    constexpr size_t MAX_FRAME_SIZE = 8 * 1024 * 1024; // 8MB
    // Maximum number of pending send jobs per connection before applying backpressure
    constexpr size_t MAX_PENDING_SEND_JOBS = 1024;
}
