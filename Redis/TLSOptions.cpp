#include "TLSOptions.h"

namespace Redis
{
	TLSOptions::TLSOptions(void) : use_tls_(false), ca_cert_(""), client_cert_(""), client_key_(""), verify_peer_(false) {}

	auto TLSOptions::use_tls(bool value) -> TLSOptions&
	{
		use_tls_ = value;
		return *this;
	}

	auto TLSOptions::use_tls() const -> bool { return use_tls_; }

	auto TLSOptions::ca_cert(const std::string& value) -> TLSOptions&
	{
		ca_cert_ = value;
		return *this;
	}

	auto TLSOptions::ca_cert() const -> const std::string& { return ca_cert_; }

	auto TLSOptions::client_cert(const std::string& value) -> TLSOptions&
	{
		client_cert_ = value;
		return *this;
	}

	auto TLSOptions::client_cert() const -> const std::string& { return client_cert_; }

	auto TLSOptions::client_key(const std::string& value) -> TLSOptions&
	{
		client_key_ = value;
		return *this;
	}

	auto TLSOptions::client_key() const -> const std::string& { return client_key_; }

	auto TLSOptions::verify_peer(bool value) -> TLSOptions&
	{
		verify_peer_ = value;
		return *this;
	}

	auto TLSOptions::verify_peer() const -> bool { return verify_peer_; }
}