#pragma once

#include "DataModes.h"
#include "ThreadPool.h"
#include "JobPriorities.h"
#include "ConnectConditions.h"

#include "boost/asio.hpp"
#include <boost/asio/strand.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>

#include <memory>
#include <vector>
#include <expected>

namespace Network
{
	class DataHandler
	{
	public:
		DataHandler(uint16_t high_priority_count, uint16_t normal_priority_count, uint16_t low_priority_count);
		virtual ~DataHandler(void);

		auto id(const std::string& new_id) -> void;

		auto id(void) const -> std::string;
		auto sub_id(void) const -> std::string;

		auto start_code(char code1, char code2, char code3, char code4) -> void;
		auto end_code(char code1, char code2, char code3, char code4) -> void;

		auto condition(ConnectConditions new_condition, bool by_itself = false) -> void;
		auto condition(void) -> const ConnectConditions;

#ifdef USE_ENCRYPT_MODULE
		auto encrypt_mode(void) -> const bool;
#endif

		auto send_binary(const std::vector<uint8_t>& binary, const std::string& message) -> std::expected<void, std::string>;
		auto send_message(const std::string& message) -> std::expected<void, std::string>;
		auto send_files(const std::vector<std::pair<std::string, std::string>>& file_informations) -> std::expected<void, std::string>;

#ifdef USE_ENCRYPT_MODULE
	protected:
		auto encrypt_mode(bool mode) -> void;

		auto key(const std::string& key_value) -> void;
		auto key(void) -> const std::string;

		auto iv(const std::string& iv_value) -> void;
		auto iv(void) -> const std::string;
#endif

		auto save_temp_path(const std::vector<uint8_t> data) -> std::optional<std::string>;

	protected:
		// Strand to serialize all socket IO handlers
		auto strand(void) -> std::shared_ptr<boost::asio::strand<boost::asio::any_io_executor>>;

		// Set a weak self guard to avoid use-after-free in async handlers
		auto set_self_guard(const std::weak_ptr<void>& self) -> void;
		auto alive(void) const -> bool;

	protected:
		auto sub_id(const std::string& new_id) -> void;

		auto create_thread_pool(const std::string& thread_pool_title) -> void;
		auto thread_pool(void) -> std::shared_ptr<Thread::ThreadPool>;
		auto destroy_thread_pool(void) -> void;

		auto buffer_size(size_t size) -> void;
		auto buffer_size(void) const -> size_t;

		auto socket(std::shared_ptr<boost::asio::ip::tcp::socket> new_socket) -> void;
		auto socket(void) -> std::shared_ptr<boost::asio::ip::tcp::socket>;
		auto destroy_socket(void) -> void;

		auto send(DataModes mode, const std::vector<uint8_t>& data) -> std::expected<void, std::string>;

		auto read_start_code(uint8_t matched_index = 0) -> void;
		auto read_length_code(void) -> void;
		auto read_data(size_t remained_data_length) -> void;
		auto read_end_code(uint8_t matched_index = 0) -> void;

		virtual auto disconnected(bool by_itself) -> void = 0;
		virtual auto received_data(DataModes mode, const std::vector<uint8_t>& data) -> std::expected<void, std::string> = 0;

	private:
		auto create_receiving_buffers(size_t size) -> void;
		auto destroy_receiving_buffers(void) -> void;

		auto compress_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
		auto decompress_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;

#ifdef USE_ENCRYPT_MODULE
		auto encrypt_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
		auto decrypt_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>;
#endif

	protected:
		std::mutex mutex_;

	private:
		std::string id_;
		std::string sub_id_;
		size_t buffer_size_;
		ConnectConditions condition_;
		uint16_t high_priority_count_;
		uint16_t normal_priority_count_;
		uint16_t low_priority_count_;

#ifdef USE_ENCRYPT_MODULE
		std::string key_;
		std::string iv_;

		bool encrypt_mode_;
#endif

		std::shared_ptr<Thread::ThreadPool> thread_pool_;
		std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
		std::shared_ptr<boost::asio::strand<boost::asio::any_io_executor>> strand_;
		std::weak_ptr<void> self_guard_;

		uint8_t* receiving_buffers_;
		std::vector<uint8_t> start_code_tag_;
		std::vector<uint8_t> end_code_tag_;
		std::vector<uint8_t> received_data_;
	};
}
