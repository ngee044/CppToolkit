#include "SSLOptions.h"

namespace RabbitMQ
{
	SSLOptions::SSLOptions(void) : use_ssl_(false), ca_cert_(""), client_cert_(""), client_key_(""), engine_(""), verify_peer_(false), verify_hostname_(false) {}

	auto SSLOptions::use_ssl(bool value) -> SSLOptions&
	{
		use_ssl_ = value;
		return *this;
	}

	auto SSLOptions::use_ssl() const -> bool { return use_ssl_; }

	auto SSLOptions::ca_cert(const std::string& value) -> SSLOptions&
	{
		ca_cert_ = value;
		return *this;
	}

	auto SSLOptions::ca_cert() const -> std::string { return ca_cert_; }

	auto SSLOptions::client_cert(const std::string& value) -> SSLOptions&
	{
		client_cert_ = value;
		return *this;
	}

	auto SSLOptions::client_cert() const -> const std::string& { return client_cert_; }

	auto SSLOptions::client_key(const std::string& value) -> SSLOptions&
	{
		client_key_ = value;
		return *this;
	}

	auto SSLOptions::client_key() const -> const std::string& { return client_key_; }

	auto SSLOptions::engine(const std::string& value) -> SSLOptions&
	{
		engine_ = value;
		return *this;
	}

	auto SSLOptions::engine() const -> const std::string& { return engine_; }

	auto SSLOptions::verify_peer(bool value) -> SSLOptions&
	{
		verify_peer_ = value;
		return *this;
	}

	auto SSLOptions::verify_peer() const -> bool { return verify_peer_; }

	auto SSLOptions::verify_hostname(bool value) -> SSLOptions&
	{
		verify_hostname_ = value;
		return *this;
	}

	auto SSLOptions::verify_hostname() const -> bool { return verify_hostname_; }
}