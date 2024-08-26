#include "NetworkServer.h"

#include "Job.h"
#include "Logger.h"
#include "Converter.h"
#include "ThreadWorker.h"
#include "NetworkSession.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

#include "boost/json.hpp"

#include <functional>

using namespace Utilities;

namespace Network
{
	NetworkServer::NetworkServer(const std::string& id, const uint16_t& high_priority_count, const uint16_t& normal_priority_count, const uint16_t& low_priority_count)
		: id_(id)
		, high_priority_count_(high_priority_count)
		, normal_priority_count_(normal_priority_count)
		, buffer_size_(1024)
		, low_priority_count_(low_priority_count)
		, io_context_(nullptr)
		, acceptor_(nullptr)
		, promise_status_(nullptr)
#ifdef USE_ENCRYPT_MODULE
		, encrypt_mode_(false)
#endif
	{
	}

	NetworkServer::~NetworkServer(void)
	{
		drop_sessions();
		destroy_io_context();

		Logger::handle().write(LogTypes::Sequence, fmt::format("destroyed NetworkServer on {}", id()));
	}

	auto NetworkServer::get_ptr(void) -> std::shared_ptr<NetworkServer> { return shared_from_this(); }

	auto NetworkServer::id(const std::string& new_id) -> void { id_ = new_id; }

	auto NetworkServer::id(void) const -> std::string { return id_; }

#ifdef USE_ENCRYPT_MODULE
	auto NetworkServer::encrypt_mode(const bool& mode) -> void { encrypt_mode_ = mode; }

	auto NetworkServer::encrypt_mode(void) -> const bool { return encrypt_mode_; }
#endif

	auto NetworkServer::register_key(const std::string& key) -> void { registered_key_ = key; }

	auto NetworkServer::start(const uint16_t& port, const size_t& socket_buffer_size) -> std::tuple<bool, std::optional<std::string>>
	{
		stop();

		buffer_size_ = socket_buffer_size;

		if (!create_io_context(port))
		{
			stop();

			return { false, "" };
		}

		// Make sure the context starts to run AFTER calling async_accept on acceptor
		wait_connection();
		start_main_job();

		return { true, std::nullopt };
	}

	auto NetworkServer::send_binary(const std::vector<uint8_t>& binary,
									const std::string& message,
									const std::string& id,
									const std::string& sub_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::unique_lock lock(mutex_);
		auto& sessions = sessions_;
		lock.unlock();

		for (auto& session : sessions)
		{
			if (session == nullptr)
			{
				continue;
			}

			if (id.empty())
			{
				auto [send_result, send_error] = session->send_binary(binary, message);
				if (!send_result)
				{
					return { send_result, send_error };
				}

				continue;
			}

			if (id != session->id())
			{
				continue;
			}

			if (!sub_id.empty() && sub_id != session->sub_id())
			{
				continue;
			}

			auto [send_result, send_error] = session->send_binary(binary, message);
			if (!send_result)
			{
				return { send_result, send_error };
			}
		}

		return { true, std::nullopt };
	}

	auto NetworkServer::send_message(const std::string& message, const std::string& id, const std::string& sub_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::unique_lock lock(mutex_);
		auto& sessions = sessions_;
		lock.unlock();

		for (auto& session : sessions)
		{
			if (session == nullptr)
			{
				continue;
			}

			if (id.empty())
			{
				auto [send_result, send_error] = session->send_message(message);
				if (!send_result)
				{
					return { send_result, send_error };
				}

				continue;
			}

			if (id != session->id())
			{
				continue;
			}

			if (!sub_id.empty() && sub_id != session->sub_id())
			{
				continue;
			}

			auto [send_result, send_error] = session->send_message(message);
			if (!send_result)
			{
				return { send_result, send_error };
			}
		}

		return { true, std::nullopt };
	}

	auto NetworkServer::send_files(const std::vector<std::pair<std::string, std::string>>& file_informations,
								   const std::string& id,
								   const std::string& sub_id) -> std::tuple<bool, std::optional<std::string>>
	{
		std::unique_lock lock(mutex_);
		auto& sessions = sessions_;
		lock.unlock();

		for (auto& session : sessions)
		{
			if (session == nullptr)
			{
				continue;
			}

			if (id.empty())
			{
				auto [send_result, send_error] = session->send_files(file_informations);
				if (!send_result)
				{
					return { send_result, send_error };
				}

				continue;
			}

			if (id != session->id())
			{
				continue;
			}

			if (!sub_id.empty() && sub_id != session->sub_id())
			{
				continue;
			}

			auto [send_result, send_error] = session->send_files(file_informations);
			if (!send_result)
			{
				return { send_result, send_error };
			}
		}

		return { true, std::nullopt };
	}

	auto NetworkServer::wait_stop(const uint32_t& seconds) -> std::tuple<bool, std::optional<std::string>>
	{
		if (io_context_ == nullptr)
		{
			return { false, "io_context is null" };
		}

		if (promise_status_ == nullptr)
		{
			promise_status_ = std::make_unique<std::promise<bool>>();
		}

		future_status_ = promise_status_->get_future();
		if (!future_status_.valid())
		{
			promise_status_.reset();

			return { false, "cannot wait to stop due to invalid future status" };
		}

		if (seconds == 0)
		{
			Logger::handle().write(LogTypes::Debug, fmt::format("attempt to wait until stop NetworkServer on {}", id_));

			future_status_.wait();

			drop_sessions();
			destroy_io_context();

			return { true, std::nullopt };
		}

		Logger::handle().write(LogTypes::Debug, fmt::format("attempt to wait on {} seconds or until NetworkServer for {} stops", id_, seconds));

		future_status_.wait_for(std::chrono::seconds(seconds));

		drop_sessions();
		destroy_io_context();

		return { true, std::nullopt };
	}

	auto NetworkServer::stop(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (io_context_ == nullptr)
		{
			return { false, "io_context is null" };
		}

		Logger::handle().write(LogTypes::Debug, fmt::format("attempt to stop NetworkServer on {}", id_));

		if (promise_status_ != nullptr && future_status_.valid())
		{
			promise_status_->set_value(true);
			promise_status_.reset();

			return { true, std::nullopt };
		}

		drop_sessions();
		destroy_io_context();

		return { true, std::nullopt };
	}

	auto NetworkServer::received_connection_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const bool&)>& callback) -> void
	{
		received_connection_callback_ = callback;
	}

	auto NetworkServer::received_binary_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
			callback) -> void
	{
		received_binary_callback_ = callback;
	}

	auto NetworkServer::received_message_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&)>& callback) -> void
	{
		received_message_callback_ = callback;
	}

	auto NetworkServer::received_file_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&)>&
			callback) -> void
	{
		received_file_callback_ = callback;
	}

	auto NetworkServer::received_files_callback(
		const std::function<std::tuple<bool, std::optional<std::string>>(
			const std::string&, const std::string&, const std::vector<std::string>&, const std::vector<std::pair<std::string, std::string>>&)>& callback) -> void
	{
		received_files_callback_ = callback;
	}

	auto NetworkServer::drop_session(const std::string& id, const std::string& sub_id) -> void
	{
		std::scoped_lock lock(mutex_);

		std::vector<std::shared_ptr<NetworkSession>> sessions;
		sessions.swap(sessions_);

		for (auto& session : sessions)
		{
			if (session == nullptr)
			{
				continue;
			}

			if (id.compare(session->id()) == 0 && sub_id.compare(session->sub_id()) == 0)
			{
				continue;
			}

			sessions_.push_back(session);
		}

		sessions.clear();
	}

	auto NetworkServer::drop_sessions(const std::string& id) -> void
	{
		std::scoped_lock lock(mutex_);

		std::vector<std::shared_ptr<NetworkSession>> sessions;
		sessions.swap(sessions_);

		for (auto& session : sessions)
		{
			if (session == nullptr)
			{
				continue;
			}

			if (id.compare(session->id()) == 0)
			{
				continue;
			}

			sessions_.push_back(session);
		}

		sessions.clear();
	}

	auto NetworkServer::received_connection(const std::vector<uint8_t>& condition) -> std::tuple<bool, std::optional<std::string>>
	{
		if (thread_pool_ == nullptr)
		{
			return { false, "there is no thread pool handle" };
		}

		return thread_pool_->push(std::make_shared<Job>(JobPriorities::Normal, condition,
														std::bind(&NetworkServer::received_connection_handler, this, std::placeholders::_1), "received_connection_job"));
	}

	auto NetworkServer::received_binary(const std::string& id,
										const std::string& sub_id,
										const std::string& message,
										const std::vector<uint8_t>& data) -> std::tuple<bool, std::optional<std::string>>
	{
		if (received_binary_callback_ == nullptr)
		{
			return { false, "there is no callback to handle received binary" };
		}

		return received_binary_callback_(id, sub_id, message, data);
	}

	auto NetworkServer::received_message(const std::string& id, const std::string& sub_id, const std::string& message) -> std::tuple<bool, std::optional<std::string>>
	{
		if (received_message_callback_ == nullptr)
		{
			return { false, "there is no callback to handle received message" };
		}

		return received_message_callback_(id, sub_id, message);
	}

	auto NetworkServer::received_file(const std::string& id,
									  const std::string& sub_id,
									  const std::string& message,
									  const std::vector<uint8_t>& file_path) -> std::tuple<bool, std::optional<std::string>>
	{
		if (received_file_callback_ == nullptr)
		{
			return { false, "there is no callback to handle received file" };
		}

		return received_file_callback_(id, sub_id, message, file_path);
	}

	auto NetworkServer::received_files(const std::string& id,
									   const std::string& sub_id,
									   const std::vector<std::string>& failures,
									   const std::vector<std::pair<std::string, std::string>>& successes) -> std::tuple<bool, std::optional<std::string>>
	{
		if (received_files_callback_ == nullptr)
		{
			return { false, "there is no callback to handle received files" };
		}

		return received_files_callback_(id, sub_id, failures, successes);
	}

	auto NetworkServer::create_io_context(const uint16_t& port) -> bool
	{
		destroy_io_context();

		std::unique_lock<std::mutex> lock(mutex_);

		io_context_ = std::make_shared<boost::asio::io_context>();

		try
		{
			acceptor_ = std::make_shared<boost::asio::ip::tcp::acceptor>(*io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), false);
		}
		catch (const std::overflow_error& message)
		{
			acceptor_.reset();
			lock.unlock();
			destroy_io_context();
			Logger::handle().write(LogTypes::Exception, fmt::format("cannot create acceptor on NetworkServer on {} => {}", id_, message.what()));

			return false;
		}
		catch (const std::runtime_error& message)
		{
			acceptor_.reset();
			lock.unlock();
			destroy_io_context();
			Logger::handle().write(LogTypes::Exception, fmt::format("cannot create acceptor on NetworkServer on {} => {}", id_, message.what()));

			return false;
		}
		catch (const std::exception& message)
		{
			acceptor_.reset();
			lock.unlock();
			destroy_io_context();
			Logger::handle().write(LogTypes::Exception, fmt::format("cannot create acceptor on NetworkServer on {} => {}", id_, message.what()));

			return false;
		}
		catch (...)
		{
			acceptor_.reset();
			lock.unlock();
			destroy_io_context();
			Logger::handle().write(LogTypes::Exception, fmt::format("cannot create acceptor on NetworkServer on {} => unexpected error", id_));

			return false;
		}

		create_thread_pool();

		return true;
	}

	auto NetworkServer::destroy_io_context(void) -> void
	{
		std::scoped_lock<std::mutex> lock(mutex_);

		if (thread_pool_ != nullptr)
		{
			thread_pool_->lock(true);
		}

		if (acceptor_ != nullptr)
		{
			acceptor_->cancel();
			acceptor_->close();
			acceptor_.reset();
		}

		if (io_context_ != nullptr)
		{
			io_context_->stop();
			io_context_.reset();
		}

		destroy_thread_pool();
	}

	auto NetworkServer::create_thread_pool(void) -> void
	{
		destroy_thread_pool();

		thread_pool_ = std::make_shared<ThreadPool>(fmt::format("ThreadPool on NetworkServer on {}", id_));
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }));
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Low }));
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::LongTerm }));

		thread_pool_->start();
	}

	auto NetworkServer::destroy_thread_pool(void) -> void
	{
		if (thread_pool_ == nullptr)
		{
			return;
		}

		thread_pool_->stop(true);
		thread_pool_.reset();
	}

	auto NetworkServer::drop_sessions(void) -> void
	{
		std::scoped_lock lock(mutex_);

		std::vector<std::shared_ptr<NetworkSession>> sessions;
		sessions.swap(sessions_);

		bool result = false;
		for (auto& session : sessions)
		{
			if (session == nullptr)
			{
				continue;
			}

			session->received_connection_callback(nullptr);
			session->received_message_callback(nullptr);
			session->received_binary_callback(nullptr);
			session->received_file_callback(nullptr);
			session->received_files_callback(nullptr);

			session->stop();
		}

		sessions.clear();
	}

	auto NetworkServer::start_main_job(void) -> void
	{
		thread_pool_->push(std::make_shared<Job>(JobPriorities::LongTerm, std::bind(&NetworkServer::run, this), "main_job_for_server", false));
	}

	auto NetworkServer::wait_connection(void) -> void
	{
		if (acceptor_ == nullptr)
		{
			return;
		}

		acceptor_->async_accept(
			[this](boost::system::error_code ec, boost::asio::ip::tcp::socket new_socket)
			{
				if (ec)
				{
					return;
				}

#ifdef _DEBUG
				Logger::handle().write(
					LogTypes::Debug, fmt::format("accepted new client: {}:{}", new_socket.remote_endpoint().address().to_string(), new_socket.remote_endpoint().port()));
#endif

#ifdef USE_ENCRYPT_MODULE
				std::shared_ptr<NetworkSession> session
					= std::make_shared<NetworkSession>(id_, encrypt_mode_, high_priority_count_, normal_priority_count_, low_priority_count_);
#else
				std::shared_ptr<NetworkSession> session = std::make_shared<NetworkSession>(id_, high_priority_count_, normal_priority_count_, low_priority_count_);
#endif
				if (session == nullptr)
				{
					wait_connection();

					return;
				}

				session->register_key(registered_key_);
				session->received_connection_callback(std::bind(&NetworkServer::received_connection, this, std::placeholders::_1));
				session->received_binary_callback(
					std::bind(&NetworkServer::received_binary, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
				session->received_message_callback(
					std::bind(&NetworkServer::received_message, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				session->received_file_callback(
					std::bind(&NetworkServer::received_file, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
				session->received_files_callback(
					std::bind(&NetworkServer::received_files, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

				session->start(std::make_shared<boost::asio::ip::tcp::socket>(std::move(new_socket)), buffer_size_);

				sessions_.push_back(session);

				wait_connection();
			});
	}

	auto NetworkServer::received_connection_handler(const std::vector<uint8_t>& condition) -> std::tuple<bool, std::optional<std::string>>
	{
		boost::json::object condition_message = boost::json::parse(Converter::to_string(condition)).as_object();

		Logger::handle().write(LogTypes::Debug,
							   fmt::format("received connection message from NetworkSession : [{}:{}] => {}", condition_message.at("id").as_string().data(),
										   condition_message.at("sub_id").as_string().data(), condition_message.at("condition").as_bool()));

		if (!condition_message.at("condition").as_bool())
		{
			std::unique_lock lock(mutex_);
			auto iter = sessions_.begin();
			while (iter != sessions_.end())
			{
				if (*iter == nullptr)
				{
					iter = sessions_.erase(iter);

					continue;
				}

				if ((*iter)->condition() != ConnectConditions::Expired)
				{
					iter++;

					continue;
				}

				(*iter).reset();
				iter = sessions_.erase(iter);
			}
			lock.unlock();
		}

		Logger::handle().write(LogTypes::Information, fmt::format("working session count : {}", sessions_.size()));

		if (received_connection_callback_ == nullptr)
		{
			return { false, "there is no callback to handle received connection" };
		}

		return received_connection_callback_(condition_message.at("id").as_string().data(), condition_message.at("sub_id").as_string().data(),
											 condition_message.at("condition").as_bool());
	}

	auto NetworkServer::run(void) -> std::tuple<bool, std::optional<std::string>>
	{
		Logger::handle().write(LogTypes::Debug, fmt::format("started io_context on NetworkServer for {}", id_));

		try
		{
			io_context_->run();
		}
		catch (const std::overflow_error& message)
		{
			io_context_.reset();
			return { false, fmt::format("stop io_context on NetworkServer for {} => {}", id_, message.what()) };
		}
		catch (const std::runtime_error& message)
		{
			io_context_.reset();
			return { false, fmt::format("stop io_context on NetworkServer for {} => {}", id_, message.what()) };
		}
		catch (const std::exception& message)
		{
			io_context_.reset();
			return { false, fmt::format("stop io_context on NetworkServer for {} => {}", id_, message.what()) };
		}
		catch (...)
		{
			io_context_.reset();
			return { false, fmt::format("stop io_context on NetworkServer for {} => unexpected error", id_) };
		}

		Logger::handle().write(LogTypes::Debug, fmt::format("stopped io_context on NetworkServer for {}", id_));

		return { true, std::nullopt };
	}
}