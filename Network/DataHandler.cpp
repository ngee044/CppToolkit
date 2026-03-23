#include "DataHandler.h"

#include "Job.h"
#include "File.h"
#include "Logger.h"
#include "Combiner.h"
#include "Converter.h"
#include "Encryptor.h"
#include "Generator.h"
#include "Compressor.h"
#include "SendingJob.h"
#include "ReceivingJob.h"
#include "ThreadWorker.h"
#include "JobPool.h"
#include "FileSendingJob.h"
#include "NetworkConstexpr.h"

#include <format>
#include <expected>

#include <filesystem>

using namespace Thread;
using namespace Utilities;

namespace Network
{
	DataHandler::DataHandler(uint16_t high_priority_count, uint16_t normal_priority_count, uint16_t low_priority_count)
		: high_priority_count_(high_priority_count)
		, normal_priority_count_(normal_priority_count)
		, low_priority_count_(low_priority_count)
		, condition_(ConnectConditions::None)
		, socket_(nullptr)
		, strand_(nullptr)
		, id_("")
		, buffer_size_(1024)
		, receiving_buffers_(nullptr)
		, thread_pool_(nullptr)
#ifdef USE_ENCRYPT_MODULE
		, encrypt_mode_(false)
		, key_("")
		, iv_("")
#endif
	{
		create_receiving_buffers(buffer_size_);

		start_code((char)253, (char)253, (char)253, (char)253);
		end_code((char)252, (char)252, (char)252, (char)252);
	}

	DataHandler::~DataHandler(void)
	{
		destroy_receiving_buffers();

		Logger::handle().write(LogTypes::Sequence, std::format("destroyed NetworkClient on {}", id()));
	}

	auto DataHandler::id(const std::string& new_id) -> void
	{
		id_ = new_id;

		if (thread_pool_)
		{
			thread_pool_->thread_title(new_id);
		}
	}

auto DataHandler::id(void) const -> std::string { return id_; }

auto DataHandler::sub_id(void) const -> std::string { return sub_id_; }

auto DataHandler::strand(void) -> std::shared_ptr<boost::asio::strand<boost::asio::any_io_executor>> { return strand_; }

auto DataHandler::set_self_guard(const std::weak_ptr<void>& self) -> void { self_guard_ = self; }

auto DataHandler::alive(void) const -> bool { return !self_guard_.expired(); }

	auto DataHandler::start_code(char code1, char code2, char code3, char code4) -> void
	{
		start_code_tag_.clear();

		start_code_tag_.push_back(code1);
		start_code_tag_.push_back(code2);
		start_code_tag_.push_back(code3);
		start_code_tag_.push_back(code4);
	}

	auto DataHandler::end_code(char code1, char code2, char code3, char code4) -> void
	{
		end_code_tag_.clear();

		end_code_tag_.push_back(code1);
		end_code_tag_.push_back(code2);
		end_code_tag_.push_back(code3);
		end_code_tag_.push_back(code4);
	}

#ifdef USE_ENCRYPT_MODULE
	auto DataHandler::encrypt_mode(void) -> const bool { return encrypt_mode_; }
#endif

	auto DataHandler::send_binary(const std::vector<uint8_t>& binary, const std::string& message) -> std::expected<void, std::string>
	{
		if (condition_ != ConnectConditions::Confirmed)
		{
			return std::unexpected(std::format("cannot send binary due to connect condition on {}: not confirmed", id()));
		}

		std::vector<uint8_t> data;
		Combiner::append(data, Converter::to_array(message));
		Combiner::append(data, binary);

		return send(DataModes::Binary, data);
	}

	auto DataHandler::send_message(const std::string& message) -> std::expected<void, std::string>
	{
		if (condition_ != ConnectConditions::Confirmed)
		{
			return std::unexpected(std::format("cannot send message due to connect condition on {}: not confirmed", id()));
		}

		return send(DataModes::Message, Converter::to_array(message));
	}

	auto DataHandler::send_files(const std::vector<std::pair<std::string, std::string>>& file_informations) -> std::expected<void, std::string>
	{
		if (condition_ != ConnectConditions::Confirmed)
		{
			return std::unexpected(std::format("cannot send files due to connect condition on {}: not confirmed", id()));
		}

		std::string guid = Generator::guid();

		std::vector<uint8_t> file_count;
		size_t size = file_informations.size();
		file_count.insert(file_count.end(), reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size) + sizeof(size_t));

		std::vector<uint8_t> start_code;
		Combiner::append(start_code, Converter::to_array(guid));
		Combiner::append(start_code, { (uint8_t)FileModes::Start });
		Combiner::append(start_code, file_count);

		auto start_result = send(DataModes::File, start_code);
		if (!start_result)
		{
			return std::unexpected(start_result.error());
		}

		size_t index = 0;
		for (const auto& [file_path, message] : file_informations)
		{
			file_count.clear();
			file_count.insert(file_count.end(), reinterpret_cast<uint8_t*>(&index), reinterpret_cast<uint8_t*>(&index) + sizeof(size_t));
			index++;

			std::vector<uint8_t> file_data;
			Combiner::append(file_data, Converter::to_array(guid));
			Combiner::append(file_data, file_count);
			Combiner::append(file_data, Converter::to_array(file_path));
			Combiner::append(file_data, Converter::to_array(message));

			auto push_result = thread_pool_->push(std::dynamic_pointer_cast<Job>(
				std::make_shared<FileSendingJob>(file_data, std::bind(&DataHandler::send, this, std::placeholders::_1, std::placeholders::_2))));
			if (!push_result)
			{
				return std::unexpected(push_result.error());
			}
		}

		return {};
	}

#ifdef USE_ENCRYPT_MODULE
	auto DataHandler::encrypt_mode(bool mode) -> void { encrypt_mode_ = mode; }

	auto DataHandler::key(const std::string& key_value) -> void { key_ = key_value; }

	auto DataHandler::key(void) -> const std::string { return key_; }

	auto DataHandler::iv(const std::string& iv_value) -> void { iv_ = iv_value; }

	auto DataHandler::iv(void) -> const std::string { return iv_; }
#endif

	auto DataHandler::save_temp_path(const std::vector<uint8_t> data) -> std::optional<std::string>
	{
		auto temp_path = std::filesystem::temp_directory_path();

		temp_path.append(Generator::guid());

		std::string temp_file_path = temp_path.string();

		File temp_file;
		auto open_result = temp_file.open(temp_file_path, std::ios::out | std::ios::binary | std::ios::app, std::locale());
		if (!open_result)
		{
			temp_file.close();

			Logger::handle().write(LogTypes::Error, std::format("cannot create temp file: {}", open_result.error()));

			return std::nullopt;
		}

		auto write_result = temp_file.write_bytes(data);
		if (!write_result)
		{
			temp_file.close();

			Logger::handle().write(LogTypes::Error, std::format("cannot write temp file: {}", write_result.error()));

			return std::nullopt;
		}

		temp_file.close();

		return temp_file_path;
	}

	auto DataHandler::sub_id(const std::string& new_id) -> void { sub_id_ = new_id; }

	auto DataHandler::create_thread_pool(const std::string& thread_pool_title) -> void
	{
		destroy_thread_pool();

		std::scoped_lock<std::mutex> lock(mutex_);

		thread_pool_ = std::make_shared<ThreadPool>(thread_pool_title);
		thread_pool_->thread_title(id_);

		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Top }));
		for (uint16_t i = 0; i < high_priority_count_; ++i)
		{
			thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }));
		}
		for (uint16_t i = 0; i < normal_priority_count_; ++i)
		{
			thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal, JobPriorities::High }));
		}
		for (uint16_t i = 0; i < low_priority_count_; ++i)
		{
			thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Low, JobPriorities::High, JobPriorities::Normal }));
		}
		thread_pool_->start();
	}

	std::shared_ptr<ThreadPool> DataHandler::thread_pool(void) { return thread_pool_; }

	auto DataHandler::destroy_thread_pool(void) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		if (thread_pool_ == nullptr)
		{
			return;
		}

		Logger::handle().write(LogTypes::Sequence, "attempt to call destroy_thread_pool on DataHandler");

		thread_pool_->stop(true);
		thread_pool_.reset();
	}

	auto DataHandler::buffer_size(size_t size) -> void
	{
		if (buffer_size_ != size || receiving_buffers_ == nullptr)
		{
			create_receiving_buffers(size);
		}

		buffer_size_ = size;
	}

	auto DataHandler::buffer_size(void) const -> size_t { return buffer_size_; }

auto DataHandler::socket(std::shared_ptr<boost::asio::ip::tcp::socket> new_socket) -> void
{
    socket_ = new_socket;
    if (socket_ && socket_->is_open())
    {
        try
        {
            strand_ = std::make_shared<boost::asio::strand<boost::asio::any_io_executor>>(socket_->get_executor());
        }
        catch (...)
        {
            strand_.reset();
        }
    }
    else
    {
        strand_.reset();
    }
}

	auto DataHandler::socket(void) -> std::shared_ptr<boost::asio::ip::tcp::socket> { return socket_; }

	auto DataHandler::destroy_socket(void) -> void
	{
		if (socket_ == nullptr)
		{
			return;
		}

		if (!socket_->is_open())
		{
			socket_.reset();

			return;
		}

		Logger::handle().write(LogTypes::Sequence, "attempt to call destroy_socket on DataHandler");

		boost::system::error_code ec;

		socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		ec.clear();

		socket_->close(ec);
		ec.clear();

		socket_.reset();
		strand_.reset();
	}

	auto DataHandler::condition(ConnectConditions new_condition, bool by_itself) -> void
	{
		if (condition_ == new_condition)
		{
			return;
		}

		condition_ = new_condition;

#ifdef _DEBUG
		Logger::handle().write(LogTypes::Debug, std::format("connection condition : {}", (uint8_t)condition_));
#endif

		if (condition_ == ConnectConditions::Expired)
		{
			disconnected(by_itself);
		}
	}

	auto DataHandler::condition(void) -> const ConnectConditions { return condition_; }

	auto DataHandler::send(DataModes mode, const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (socket_ == nullptr)
		{
			return std::unexpected(std::format("cannot send a message by null socket : mode[{}]", (uint8_t)mode));
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected(std::format("cannot send a message by null thread pool : mode[{}]", (uint8_t)mode));
		}

		if (data.empty())
		{
			return std::unexpected(std::format("cannot send a message by null data : mode[{}]", (uint8_t)mode));
		}

		// Apply simple backpressure: cap total pending jobs except for Connection control messages
		if (mode != DataModes::Connection)
		{
			auto pool = thread_pool_;
			if (pool && pool->job_pool())
			{
				std::vector<JobPriorities> all; // empty => count all
				size_t pending = pool->job_pool()->job_count(all);
				if (pending >= MAX_PENDING_SEND_JOBS)
				{
					return std::unexpected(std::format("backpressure: too many pending send jobs ({} >= {})", pending, MAX_PENDING_SEND_JOBS));
				}
			}
		}

		std::vector<uint8_t> sending_data;
		Combiner::append(sending_data, { (uint8_t)mode });
		Combiner::append(sending_data, data);

#ifdef USE_ENCRYPT_MODULE
		if (mode == DataModes::Connection)
		{
			auto result = thread_pool_->push(
				std::make_shared<Job>(JobPriorities::High, sending_data, std::bind(&DataHandler::compress_message, this, std::placeholders::_1), "compress_message"));
			return result;
		}

		{
			auto result = thread_pool_->push(
				std::make_shared<Job>(JobPriorities::Normal, sending_data, std::bind(&DataHandler::encrypt_message, this, std::placeholders::_1), "encrypt_message"));
			return result;
		}
#else
		{
			auto result = thread_pool_->push(
				std::make_shared<Job>(JobPriorities::High, sending_data, std::bind(&DataHandler::compress_message, this, std::placeholders::_1), "compress_message"));
			return result;
		}
#endif
	}

	auto DataHandler::read_start_code(uint8_t matched_index) -> void
	{
		if (condition() == ConnectConditions::Expired)
		{
			return;
		}

		if (socket_ == nullptr || !socket_->is_open())
		{
			condition(ConnectConditions::Expired);
			return;
		}

#ifdef _DEBUG
		if (matched_index == 0)
		{
			Logger::handle().write(LogTypes::Debug, "attempt to start read message on network");
		}
#endif

		if (matched_index == 4)
		{
#ifdef _DEBUG
			Logger::handle().write(LogTypes::Debug, std::format("read start code : {} bytes", START_CODE_SIZE));
#endif

			read_length_code();

			return;
		}

		received_data_.clear();
		memset(receiving_buffers_, 0, sizeof(uint8_t) * buffer_size_);
		auto handler_start = [this, matched_index](boost::system::error_code ec, size_t length)
		{
			if (!alive())
			{
				return;
			}
									if (condition() == ConnectConditions::Expired)
									{
										return;
									}

									if (ec)
									{
										condition(ConnectConditions::Expired);
										Logger::handle().write(LogTypes::Debug, std::format("expired connection : {}", ec.message()));

										return;
									}

									if (length != 1)
									{
										read_start_code();

										return;
									}

									if (receiving_buffers_[0] != start_code_tag_[matched_index])
									{
#ifdef _DEBUG
										Logger::handle().write(LogTypes::Warning, std::format("received unknown data on network : {}", receiving_buffers_[0]));
#endif

										read_start_code();

										return;
									}

									read_start_code(matched_index + 1);
								};

		if (strand_)
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, 1), boost::asio::transfer_exactly(1),
									  boost::asio::bind_executor(*strand_, std::move(handler_start)));
		}
		else
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, 1), boost::asio::transfer_exactly(1), handler_start);
		}
	}

	auto DataHandler::read_length_code(void) -> void
	{
		if (condition() == ConnectConditions::Expired)
		{
			return;
		}

		if (socket_ == nullptr || !socket_->is_open())
		{
			condition(ConnectConditions::Expired);
			return;
		}

		memset(receiving_buffers_, 0, sizeof(uint8_t) * buffer_size_);
		auto handler_length = [this](std::error_code ec, size_t length)
		{
			if (!alive())
			{
				return;
			}
									if (condition() == ConnectConditions::Expired)
									{
										return;
									}

									if (ec)
									{
										condition(ConnectConditions::Expired);
										Logger::handle().write(LogTypes::Debug, std::format("expired connection : {}", ec.message()));

										return;
									}

									if (length != LENGTH_SIZE)
									{
										Logger::handle().write(LogTypes::Warning, "drop read data: not matched length code");

										read_start_code();

										return;
									}

#ifdef _DEBUG
									Logger::handle().write(LogTypes::Debug, std::format("read length code : {} bytes", LENGTH_SIZE));
#endif

            uint64_t target_length = 0;
            memcpy(&target_length, receiving_buffers_, length);

            // Enforce maximum frame size to protect memory and stability
            if (target_length > MAX_FRAME_SIZE)
            {
                Logger::handle().write(LogTypes::Error,
                                       std::format("drop frame: declared length {} exceeds MAX_FRAME_SIZE {}", target_length, MAX_FRAME_SIZE));
                condition(ConnectConditions::Expired);
                return;
            }

            read_data(target_length);
        };

		if (strand_)
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, LENGTH_SIZE), boost::asio::transfer_exactly(LENGTH_SIZE),
									  boost::asio::bind_executor(*strand_, std::move(handler_length)));
		}
		else
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, LENGTH_SIZE), boost::asio::transfer_exactly(LENGTH_SIZE), handler_length);
		}
	}

	auto DataHandler::read_data(size_t remained_data_length) -> void
	{
		if (condition() == ConnectConditions::Expired)
		{
			return;
		}

		if (socket_ == nullptr || !socket_->is_open())
		{
			condition(ConnectConditions::Expired);
			return;
		}

		if (remained_data_length == 0)
		{
			read_end_code();

			return;
		}

		memset(receiving_buffers_, 0, sizeof(uint8_t) * buffer_size_);

		if (remained_data_length >= buffer_size_)
		{
			auto handler_chunk = [this, remained_data_length](boost::system::error_code ec, size_t length)
			{
				if (!alive())
				{
					return;
				}
										if (condition() == ConnectConditions::Expired)
										{
											return;
										}

										if (ec)
										{
											condition(ConnectConditions::Expired);
											Logger::handle().write(LogTypes::Debug, std::format("expired connection : {}", ec.message()));

											return;
										}

										if (length != buffer_size_)
										{
											Logger::handle().write(LogTypes::Warning, "drop read data: not matched data length");

											read_start_code();

											return;
										}

										received_data_.insert(received_data_.end(), receiving_buffers_, receiving_buffers_ + length);
										read_data(remained_data_length - length);
			};

			if (strand_)
			{
				boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, buffer_size_), boost::asio::transfer_exactly(buffer_size_),
										  boost::asio::bind_executor(*strand_, std::move(handler_chunk)));
			}
			else
			{
				boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, buffer_size_), boost::asio::transfer_exactly(buffer_size_), handler_chunk);
			}

			return;
		}

		auto handler_last = [this](boost::system::error_code ec, size_t length)
		{
			if (!alive())
			{
				return;
			}
									if (condition() == ConnectConditions::Expired)
									{
										return;
									}

									if (ec)
									{
										condition(ConnectConditions::Expired);
										Logger::handle().write(LogTypes::Debug, std::format("expired connection : {}", ec.message()));

										return;
									}

									received_data_.insert(received_data_.end(), receiving_buffers_, receiving_buffers_ + length);

#ifdef _DEBUG
									Logger::handle().write(LogTypes::Debug, std::format("read data : {} bytes", received_data_.size()));
#endif

									read_end_code();
		};

		if (strand_)
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, remained_data_length), boost::asio::transfer_exactly(remained_data_length),
									  boost::asio::bind_executor(*strand_, std::move(handler_last)));
		}
		else
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, remained_data_length), boost::asio::transfer_exactly(remained_data_length), handler_last);
		}
	}

	auto DataHandler::read_end_code(uint8_t matched_index) -> void
	{
		if (condition() == ConnectConditions::Expired)
		{
			return;
		}

		if (socket_ == nullptr || !socket_->is_open())
		{
			condition(ConnectConditions::Expired);
			return;
		}

		if (matched_index == end_code_tag_.size())
		{
#ifdef _DEBUG
			Logger::handle().write(LogTypes::Debug, std::format("read end code : {} bytes", end_code_tag_.size()));
#endif

			if (thread_pool_ != nullptr)
			{
				thread_pool_->push(std::make_shared<Job>(JobPriorities::Low, received_data_, std::bind(&DataHandler::decompress_message, this, std::placeholders::_1),
														 "decompress_message"));
			}

			received_data_.clear();

			read_start_code();

			return;
		}

		memset(receiving_buffers_, 0, sizeof(uint8_t) * buffer_size_);

		auto handler_end = [this, matched_index](boost::system::error_code ec, size_t length)
		{
			if (!alive())
			{
				return;
			}
									if (condition() == ConnectConditions::Expired)
									{
										return;
									}

									if (ec)
									{
										condition(ConnectConditions::Expired);
										Logger::handle().write(LogTypes::Debug, std::format("expired connection : {}", ec.message()));

										return;
									}

									if (length != 1 || receiving_buffers_[0] != end_code_tag_[matched_index])
									{
										Logger::handle().write(LogTypes::Warning, "drop read data : not matched end code");

										read_start_code();

										return;
									}

									read_end_code(matched_index + 1);
		};

		if (strand_)
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, 1), boost::asio::transfer_exactly(1),
									  boost::asio::bind_executor(*strand_, std::move(handler_end)));
		}
		else
		{
			boost::asio::async_read(*socket_, boost::asio::buffer(receiving_buffers_, 1), boost::asio::transfer_exactly(1), handler_end);
		}
	}

	auto DataHandler::create_receiving_buffers(size_t size) -> void
	{
		destroy_receiving_buffers();

		receiving_buffers_ = new uint8_t[size];
		memset(receiving_buffers_, 0, sizeof(uint8_t) * size);
	}

	auto DataHandler::destroy_receiving_buffers(void) -> void
	{
		if (receiving_buffers_ == nullptr)
		{
			return;
		}

		delete[] receiving_buffers_;
		receiving_buffers_ = nullptr;
	}

	auto DataHandler::compress_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() == ConnectConditions::Expired)
		{
			return std::unexpected("connection has expired");
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected("thread pool has no handle");
		}

		auto [buffer, message] = Compressor::compression(data);
		if (buffer == std::nullopt)
		{
			buffer = data;
		}

		auto result = thread_pool_->push(std::dynamic_pointer_cast<Job>(std::make_shared<SendingJob>(socket_, start_code_tag_, buffer.value(), end_code_tag_, buffer_size_)));
		if (!result) { return std::unexpected(result.error()); }
		return {};
	}

	auto DataHandler::decompress_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() == ConnectConditions::Expired)
		{
			return std::unexpected("connection has expired");
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected("thread pool has no handle");
		}

		auto [buffer, message] = Compressor::decompression(data);
		if (buffer == std::nullopt)
		{
			buffer = data;
		}

#ifdef USE_ENCRYPT_MODULE
		{
			auto result = thread_pool_->push(
				std::make_shared<Job>(JobPriorities::Normal, buffer.value(), std::bind(&DataHandler::decrypt_message, this, std::placeholders::_1), "decrypt_message"));
			return result;
		}
#else
		{
			auto result = thread_pool_->push(std::dynamic_pointer_cast<Job>(
				std::make_shared<ReceivingJob>(buffer.value(), std::bind(&DataHandler::received_data, this, std::placeholders::_1, std::placeholders::_2))));
			return result;
		}
#endif
	}

#ifdef USE_ENCRYPT_MODULE
	auto DataHandler::encrypt_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() == ConnectConditions::Expired)
		{
			return std::unexpected("connection has expired");
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected("thread pool has no handle");
		}

		if (!encrypt_mode_)
		{
			auto result = thread_pool_->push(
				std::make_shared<Job>(JobPriorities::High, data, std::bind(&DataHandler::compress_message, this, std::placeholders::_1), "compress_message"));
			return result;
		}

		auto [buffer, message] = Encryptor::encryption(data, key(), iv());
		if (buffer == std::nullopt)
		{
			buffer = data;
		}

		{
			auto result = thread_pool_->push(
				std::make_shared<Job>(JobPriorities::High, buffer.value(), std::bind(&DataHandler::compress_message, this, std::placeholders::_1), "compress_message"));
			return result;
		}
	}

	auto DataHandler::decrypt_message(const std::vector<uint8_t>& data) -> std::expected<void, std::string>
	{
		if (condition() == ConnectConditions::Expired)
		{
			return std::unexpected("connection has expired");
		}

		if (thread_pool_ == nullptr)
		{
			return std::unexpected("thread pool has no handle");
		}

		if (!encrypt_mode_)
		{
			auto result = thread_pool_->push(std::dynamic_pointer_cast<Job>(
				std::make_shared<ReceivingJob>(data, std::bind(&DataHandler::received_data, this, std::placeholders::_1, std::placeholders::_2))));
			return result;
		}

		auto [buffer, message] = Encryptor::decryption(data, key(), iv());
		if (buffer == std::nullopt)
		{
			buffer = data;
		}

		{
			auto result = thread_pool_->push(std::dynamic_pointer_cast<Job>(
				std::make_shared<ReceivingJob>(buffer.value(), std::bind(&DataHandler::received_data, this, std::placeholders::_1, std::placeholders::_2))));
			return result;
		}
	}
#endif
}
