#pragma once

#include <tuple>
#include <memory>
#include <string>
#include <optional>

namespace RabbitMQ
{
	class SSLOptions
	{
	public:
		SSLOptions();

		auto use_ssl(bool value) -> SSLOptions&;
		auto use_ssl() const -> bool;

		auto ca_cert(const std::string& value) -> SSLOptions&;
		auto ca_cert() const -> std::string;
	
		auto client_cert(const std::string& value) -> SSLOptions&;
		auto client_cert() const -> const std::string&;

		auto client_key(const std::string& value) -> SSLOptions&;
		auto client_key() const -> const std::string&;

		auto engine(const std::string& value) -> SSLOptions&;
		auto engine() const -> const std::string&;

		auto verify_peer(bool value) -> SSLOptions&;
		auto verify_peer() const -> bool;

		auto verify_hostname(bool value) -> SSLOptions&;
		auto verify_hostname() const -> bool;

	private:
		bool use_ssl_;
		std::string ca_cert_;
		std::string client_cert_;
		std::string client_key_;
		std::string engine_;
		bool verify_peer_;
		bool verify_hostname_;
	};
}