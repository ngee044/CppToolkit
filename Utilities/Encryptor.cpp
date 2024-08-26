#include "Encryptor.h"

#ifdef USE_ENCRYPT_MODULE

#include "Converter.h"

#include "cryptopp/aes.h"
#include "cryptopp/hex.h"
#include "cryptopp/modes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/filters.h"

namespace Utilities
{
	auto Encryptor::create_key(void) -> std::pair<std::string, std::string>
	{
		CryptoPP::AutoSeededRandomPool rng;

		CryptoPP::byte key[CryptoPP::AES::MAX_KEYLENGTH];
		memset(key, 0x00, CryptoPP::AES::MAX_KEYLENGTH);
		rng.GenerateBlock(key, CryptoPP::AES::MAX_KEYLENGTH);

		CryptoPP::byte iv[CryptoPP::AES::BLOCKSIZE];
		memset(iv, 0x00, CryptoPP::AES::BLOCKSIZE);
		rng.GenerateBlock(iv, CryptoPP::AES::BLOCKSIZE);

		return { Converter::to_base64(std::vector<uint8_t>(key, key + CryptoPP::AES::MAX_KEYLENGTH)),
				 Converter::to_base64(std::vector<uint8_t>(iv, iv + CryptoPP::AES::BLOCKSIZE)) };
	}

	auto Encryptor::encryption(const std::vector<uint8_t>& original_data,
							   const std::string& key_string,
							   const std::string& iv_string)
		-> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
	{
		if (original_data.empty())
		{
			return { std::nullopt, "the data field is empty." };
		}

		if (key_string.empty() || iv_string.empty())
		{
			return { std::nullopt, "Key or IV is not provided. encryption cannot be performed." };
		}

		std::vector<uint8_t> encrypted;
		std::vector<uint8_t> key = Converter::from_base64(key_string);
		std::vector<uint8_t> iv = Converter::from_base64(iv_string);

		CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption enc;
		enc.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

		encrypted.resize(original_data.size() + CryptoPP::AES::BLOCKSIZE);
		CryptoPP::ArraySink cs(&encrypted[0], encrypted.size());

		CryptoPP::ArraySource(original_data.data(), original_data.size(), true,
							  new CryptoPP::StreamTransformationFilter(enc, new CryptoPP::Redirector(cs)));

		encrypted.resize((size_t)cs.TotalPutLength());

		return { encrypted, std::nullopt };
	}

	auto Encryptor::decryption(const std::vector<uint8_t>& encrypted_data,
							   const std::string& key_string,
							   const std::string& iv_string)
		-> std::tuple<std::optional<std::vector<uint8_t>>, std::optional<std::string>>
	{
		if (encrypted_data.empty())
		{
			return { std::nullopt, "the data field is empty." };
		}

		if (key_string.empty() || iv_string.empty())
		{
			return { std::nullopt, "Key or IV is not provided. decryption cannot be performed." };
		}

		std::vector<uint8_t> decrypted;
		std::vector<uint8_t> key = Converter::from_base64(key_string);
		std::vector<uint8_t> iv = Converter::from_base64(iv_string);

		CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption dec;
		dec.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

		decrypted.resize(encrypted_data.size());
		CryptoPP::ArraySink rs((uint8_t*)&decrypted[0], decrypted.size());

		CryptoPP::ArraySource(encrypted_data.data(), encrypted_data.size(), true,
							  new CryptoPP::StreamTransformationFilter(dec, new CryptoPP::Redirector(rs)));

		decrypted.resize((size_t)rs.TotalPutLength());

		return { decrypted, std::nullopt };
	}
}

#endif
