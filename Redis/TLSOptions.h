#pragma once

#include <tuple>
#include <memory>
#include <string>

namespace Redis
{
	class TLSOptions
	{	public:
		TLSOptions();

		auto use_tls(bool value) -> TLSOptions&;
		auto use_tls() const -> bool;
		
		// Additional getters for the needed properties
		auto enabled() const -> bool { return use_tls_; }
		auto cert_path() const -> const std::string& { return client_cert_; }
		auto key_path() const -> const std::string& { return client_key_; }
		auto ca_cert_path() const -> const std::string& { return ca_cert_; }

		auto ca_cert(const std::string& value) -> TLSOptions&;
		auto ca_cert() const -> const std::string&;

		auto client_cert(const std::string& value) -> TLSOptions&;
		auto client_cert() const -> const std::string&;

		auto client_key(const std::string& value) -> TLSOptions&;
		auto client_key() const -> const std::string&;

		auto verify_peer(bool value) -> TLSOptions&;
		auto verify_peer() const -> bool;

	private:
		bool use_tls_;
		std::string ca_cert_;
		std::string client_cert_;
		std::string client_key_;
		bool verify_peer_;
	};
}