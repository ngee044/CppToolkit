#pragma once
#include <string>
#include <chrono>
#include <optional>

namespace Kafka {

class DeliveryResult {
public:
    enum class Status { Success, Failed };

    DeliveryResult(Status status, const std::string& message, std::optional<std::string> error = std::nullopt)
		: status_(status), message_(message), error_(error), timestamp_(std::chrono::system_clock::now()) {}

    auto get_status() const -> Status { return status_; }
    auto get_message() const -> std::string { return message_; }
    auto get_error() const -> std::optional<std::string> { return error_; }
    auto get_timestamp() const -> std::chrono::system_clock::time_point { return timestamp_; }

private:
    Status status_;
    std::string message_;
    std::optional<std::string> error_;
    std::chrono::system_clock::time_point timestamp_;
};

}
