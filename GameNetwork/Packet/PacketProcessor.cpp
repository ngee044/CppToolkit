#include "PacketProcessor.h"
#include "../../Utilities/Compressor.h"
#include "../../Utilities/Encryptor.h" 
#include "../../Utilities/Converter.h"
#include "../../Utilities/Logger.h"
#include <boost/json.hpp>
#include <boost/system.hpp>
#include <zlib.h>
#include <chrono>

namespace GameNetwork
{
	PacketProcessor::PacketProcessor()
		: compression_enabled_(false)
		, encryption_enabled_(false)
		, batching_enabled_(false)
	{
		stats_.packets_processed = 0;
		stats_.packets_compressed = 0;
		stats_.packets_encrypted = 0;
		stats_.packets_batched = 0;
		stats_.compression_ratio_percent = 100;
		stats_.average_processing_time_us = 0;
	}

	PacketProcessor::~PacketProcessor() = default;

	auto PacketProcessor::serialize(const GamePacket& packet) 
		-> std::optional<std::string>
	{
		try
		{
			auto start_time = std::chrono::high_resolution_clock::now();
            
			// Convert packet to JSON string
			std::string json_str = packet.to_json();
            
			// Apply compression if enabled
			if (compression_enabled_)
			{
				std::vector<uint8_t> json_data(json_str.begin(), json_str.end());
				auto [compressed_data, error] = Utilities::Compressor::compression(json_data);
				if (compressed_data.has_value() && !compressed_data->empty())
				{
					json_str = std::string(compressed_data->begin(), compressed_data->end());
					stats_.packets_compressed++;
				}
			}
            
			// Apply encryption if enabled
#ifdef USE_ENCRYPT_MODULE
			if (encryption_enabled_ && !encryption_key_.empty())
			{
				std::vector<uint8_t> json_data(json_str.begin(), json_str.end());
				auto [encrypted_data, error] = Utilities::Encryptor::encryption(json_data, encryption_key_, "");
				if (encrypted_data.has_value() && !encrypted_data->empty())
				{
					json_str = std::string(encrypted_data->begin(), encrypted_data->end());
					stats_.packets_encrypted++;
				}
			}
#endif
            
			// Update statistics
			stats_.packets_processed++;
			auto end_time = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
			// Update average processing time
			if (stats_.packets_processed == 1)
			{
				stats_.average_processing_time_us = duration.count();
			}
			else
			{
				stats_.average_processing_time_us = 
					(stats_.average_processing_time_us * (stats_.packets_processed - 1) + duration.count()) 
					/ stats_.packets_processed;
			}
            
			return json_str;
		}
		catch (const std::exception& e)
		{
			Utilities::Logger::handle().write(Utilities::LogTypes::Error,
				"Failed to serialize packet: " + std::string(e.what()));
			return std::nullopt;
		}
	}    auto PacketProcessor::deserialize(const std::string& data)
		-> std::unique_ptr<GamePacket>
	{
		try
		{
			std::string json_str = data;
            
			// Apply decryption if enabled
#ifdef USE_ENCRYPT_MODULE
			if (encryption_enabled_ && !encryption_key_.empty())
			{
				std::vector<uint8_t> json_data(json_str.begin(), json_str.end());
				auto [decrypted_data, error] = Utilities::Encryptor::decryption(json_data, encryption_key_, "");
				if (decrypted_data.has_value() && !decrypted_data->empty())
				{
					json_str = std::string(decrypted_data->begin(), decrypted_data->end());
				}
				else
				{
					Utilities::Logger::handle().write(Utilities::LogTypes::Error,
						"Failed to decrypt packet");
					return nullptr;
				}
			}
#endif
            
			// Apply decompression if enabled
			if (compression_enabled_)
			{
				std::vector<uint8_t> json_data(json_str.begin(), json_str.end());
				auto [decompressed_data, error] = Utilities::Compressor::decompression(json_data);
				if (decompressed_data.has_value() && !decompressed_data->empty())
				{
					json_str = std::string(decompressed_data->begin(), decompressed_data->end());
				}
			}
            
			// Parse JSON to packet - Create packet based on type
			// For now, use a default packet type - should be determined from metadata or header
			PacketType default_type = PacketType::GameData;
			auto packet = std::make_unique<GamePacket>(default_type);
            
			// Parse JSON and set packet properties
			try
			{
				boost::system::error_code ec;
				boost::json::value json_value = boost::json::parse(json_str, ec);
                
				if (ec)
				{
					Utilities::Logger::handle().write(Utilities::LogTypes::Error,
						"Failed to parse JSON: " + ec.message());
					return nullptr;
				}
                
				boost::json::object const& obj = json_value.as_object();
                
				// Set basic properties
				if (obj.contains("sender_id"))
				{
					packet->set_sender_id(static_cast<uint64_t>(obj.at("sender_id").as_int64()));
				}
				if (obj.contains("target_id"))
				{
					packet->set_target_id(static_cast<uint64_t>(obj.at("target_id").as_int64()));
				}
				if (obj.contains("timestamp"))
				{
					packet->set_timestamp(static_cast<uint64_t>(obj.at("timestamp").as_int64()));
				}
				if (obj.contains("priority"))
				{
					packet->set_priority(static_cast<PacketPriority>(obj.at("priority").as_int64()));
				}
				if (obj.contains("metadata"))
				{
					boost::json::object const& metadata = obj.at("metadata").as_object();
					for (const auto& [key, value] : metadata)
					{
						packet->metadata[key] = boost::json::serialize(value);
					}
				}
				if (obj.contains("data"))
				{
					// Convert data array to payload vector
					boost::json::array const& data_array = obj.at("data").as_array();
					std::vector<uint8_t> payload_data;
					payload_data.reserve(data_array.size());
					for (const auto& byte_val : data_array)
					{
						payload_data.push_back(static_cast<uint8_t>(byte_val.as_int64()));
					}
					packet->set_payload(payload_data);
				}
                
				return packet;
			}
			catch (const std::exception& e)
			{
				Utilities::Logger::handle().write(Utilities::LogTypes::Error,
					"Exception creating packet from JSON: " + std::string(e.what()));
				return nullptr;
			}
			// }
			// if (parsed_json.isMember("sequence"))
			// {
			//     packet->set_sequence_number(parsed_json["sequence"].asUInt64());
			// }
            
			// Set payload data
			// if (parsed_json.isMember("data"))
			// {
			//     const Json::Value& data_json = parsed_json["data"];
			//     Json::StreamWriterBuilder builder;
			//     std::string data_str = Json::writeString(builder, data_json);
			//     std::vector<uint8_t> data_vec(data_str.begin(), data_str.end());
			//     packet->set_payload(data_vec);
			// }
            
			// return packet;
		}
		catch (const std::exception& e)
		{
			Utilities::Logger::handle().write(Utilities::LogTypes::Error,
				"Failed to deserialize packet: " + std::string(e.what()));
			return nullptr;
		}
	}    auto PacketProcessor::deserialize_binary(const std::vector<uint8_t>& data)
		-> std::unique_ptr<GamePacket>
	{
		std::string str_data(data.begin(), data.end());
		return deserialize(str_data);
	}

	auto PacketProcessor::set_compression_enabled(bool enabled) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		compression_enabled_ = enabled;
	}

	auto PacketProcessor::is_compression_enabled() const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return compression_enabled_;
	}

	auto PacketProcessor::set_encryption_enabled(bool enabled) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		encryption_enabled_ = enabled;
	}

	auto PacketProcessor::is_encryption_enabled() const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return encryption_enabled_;
	}

	auto PacketProcessor::set_encryption_key(const std::string& key) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		encryption_key_ = key;
	}

	auto PacketProcessor::get_stats() const -> ProcessorStats
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return stats_;
	}

	auto PacketProcessor::reset_stats() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stats_.packets_processed = 0;
		stats_.packets_compressed = 0;
		stats_.packets_encrypted = 0;
		stats_.packets_batched = 0;
		stats_.compression_ratio_percent = 100;
		stats_.average_processing_time_us = 0;
	}
}
