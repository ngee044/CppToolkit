#include "GamePacket.h"
#include <JsonTool.h>
#include <Logger.h>
#include <Converter.h>
#include <boost/json.hpp>

using namespace Utilities;

namespace GameNetwork
{
	GamePacket::GamePacket()
		: type_(PacketType::Unknown)
		, packet_type(PacketType::Unknown)
		, timestamp_(std::chrono::steady_clock::now())
		, sequence_number_(0)
		, sender_id_(0)
		, target_id_(0)
		, channel_id_(0)
		, priority_(PacketPriority::Normal)
	{
	}

	GamePacket::GamePacket(PacketType type)
		: type_(type)
		, packet_type(type)
		, timestamp_(std::chrono::steady_clock::now())
		, sequence_number_(0)
		, sender_id_(0)
		, target_id_(0)
		, channel_id_(0)
		, priority_(PacketPriority::Normal)
	{
	}

	GamePacket::~GamePacket() = default;

	auto GamePacket::set_timestamp(uint64_t timestamp) -> void
	{
		// Temporary stub implementation
	}

	auto GamePacket::get_timestamp() const -> uint64_t
	{
		// Temporary stub implementation
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			timestamp_.time_since_epoch()).count();
	}

	auto GamePacket::to_json() const -> std::string
	{
		try
		{
			boost::json::object obj;
            
			// Basic packet info
			obj["type"] = static_cast<uint16_t>(type_);
			obj["sequence"] = sequence_number_;
			obj["sender_id"] = sender_id_;
			obj["target_id"] = target_id_;
			obj["channel_id"] = channel_id_;
			obj["priority"] = static_cast<uint8_t>(priority_);
            
			// Timestamp as milliseconds since epoch
			auto time_since_epoch = timestamp_.time_since_epoch();
			auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
			obj["timestamp"] = millis;
            
			// Payload
			if (!payload_.empty())
			{
#ifdef USE_ENCRYPT_MODULE
				obj["payload"] = Utilities::Converter::to_base64(payload_);
#else
				std::string payload_str(payload_.begin(), payload_.end());
				obj["payload"] = payload_str;
#endif
			}
            
			// Custom data
			if (!custom_data_.empty())
			{
				boost::json::object custom_obj;
				for (const auto& [key, value] : custom_data_)
				{
					custom_obj[key] = value;
				}
				obj["custom"] = custom_obj;
			}
            
			return boost::json::serialize(obj);
		}
		catch (const std::exception& e)
		{
			Logger::handle().write(LogTypes::Error,
				"Failed to serialize GamePacket to JSON: " + std::string(e.what()));
			return "{}";
		}
	}

	auto GamePacket::from_json(const std::string& json_str) -> bool
	{
		try
		{
			auto parsed = boost::json::parse(json_str);
			auto& obj = parsed.as_object();
            
			// Basic packet info
			if (obj.contains("type"))
			{
				type_ = static_cast<PacketType>(obj["type"].as_int64());
			}
            
			if (obj.contains("sequence"))
			{
				sequence_number_ = obj["sequence"].as_int64();
			}
            
			if (obj.contains("sender_id"))
			{
				sender_id_ = obj["sender_id"].as_int64();
			}
            
			if (obj.contains("target_id"))
			{
				target_id_ = obj["target_id"].as_int64();
			}
            
			if (obj.contains("channel_id"))
			{
				channel_id_ = obj["channel_id"].as_int64();
			}
            
			if (obj.contains("priority"))
			{
				priority_ = static_cast<PacketPriority>(obj["priority"].as_int64());
			}
            
			// Timestamp
			if (obj.contains("timestamp"))
			{
				auto millis = obj["timestamp"].as_int64();
				timestamp_ = std::chrono::steady_clock::time_point(
					std::chrono::milliseconds(millis));
			}
            
			// Payload
			if (obj.contains("payload") && !obj["payload"].is_null())
			{
				std::string base64_payload = obj["payload"].as_string().c_str();
#ifdef USE_ENCRYPT_MODULE
				auto decoded = Utilities::Converter::from_base64(base64_payload);
				payload_ = std::vector<uint8_t>(decoded.begin(), decoded.end());
#else
				payload_ = std::vector<uint8_t>(base64_payload.begin(), base64_payload.end());
#endif
			}
            
			// Custom data
			if (obj.contains("custom") && obj["custom"].is_object())
			{
				auto& custom_obj = obj["custom"].as_object();
				for (auto& [key, value] : custom_obj)
				{
					custom_data_[key] = boost::json::serialize(value);
				}
			}
            
			return true;
		}
		catch (const std::exception& e)
		{
			Logger::handle().write(LogTypes::Error,
				"Failed to deserialize GamePacket from JSON: " + std::string(e.what()));
			return false;
		}
	}

	auto GamePacket::get_type() const -> PacketType
	{
		return type_;
	}

	auto GamePacket::set_type(PacketType type) -> void
	{
		type_ = type;
		packet_type = type;
	}

	auto GamePacket::get_sequence_number() const -> uint64_t
	{
		return sequence_number_;
	}

	auto GamePacket::set_sequence_number(uint64_t seq) -> void
	{
		sequence_number_ = seq;
	}

	auto GamePacket::get_sender_id() const -> uint64_t
	{
		return sender_id_;
	}

	auto GamePacket::set_sender_id(uint64_t id) -> void
	{
		sender_id_ = id;
	}

	auto GamePacket::get_target_id() const -> uint64_t
	{
		return target_id_;
	}

	auto GamePacket::set_target_id(uint64_t id) -> void
	{
		target_id_ = id;
	}

	auto GamePacket::get_payload() const -> const std::vector<uint8_t>&
	{
		return payload_;
	}

	auto GamePacket::set_payload(const std::vector<uint8_t>& data) -> void
	{
		payload_ = data;
	}

	auto GamePacket::set_payload(std::vector<uint8_t>&& data) -> void
	{
		payload_ = std::move(data);
	}

	// ============= Specific Packet Class Implementations =============

	// AuthenticationPacket implementation
	AuthenticationPacket::AuthenticationPacket()
		: GamePacket(PacketType::Authentication)
		, client_version_(0)
	{
	}

	auto AuthenticationPacket::account_id() const -> std::string
	{
		return account_id_;
	}

	auto AuthenticationPacket::session_token() const -> std::string
	{
		return session_token_;
	}

	auto AuthenticationPacket::client_version() const -> uint32_t
	{
		return client_version_;
	}

	auto AuthenticationPacket::set_account_id(const std::string& id) -> void
	{
		account_id_ = id;
	}

	auto AuthenticationPacket::set_session_token(const std::string& token) -> void
	{
		session_token_ = token;
	}

	auto AuthenticationPacket::set_client_version(uint32_t version) -> void
	{
		client_version_ = version;
	}

	auto AuthenticationPacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		// Add account_id
		auto account_bytes = reinterpret_cast<const uint8_t*>(account_id_.c_str());
		uint32_t account_len = static_cast<uint32_t>(account_id_.length());
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&account_len), reinterpret_cast<const uint8_t*>(&account_len) + 4);
		data.insert(data.end(), account_bytes, account_bytes + account_len);
        
		// Add session_token
		auto token_bytes = reinterpret_cast<const uint8_t*>(session_token_.c_str());
		uint32_t token_len = static_cast<uint32_t>(session_token_.length());
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&token_len), reinterpret_cast<const uint8_t*>(&token_len) + 4);
		data.insert(data.end(), token_bytes, token_bytes + token_len);
        
		// Add client_version
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&client_version_), reinterpret_cast<const uint8_t*>(&client_version_) + 4);
        
		return data;
	}

	auto AuthenticationPacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 12) return false; // Minimum size check
        
		size_t offset = 0;
        
		// Read account_id
		uint32_t account_len = *reinterpret_cast<const uint32_t*>(&data[offset]);
		offset += 4;
		if (offset + account_len > data.size()) return false;
		account_id_ = std::string(reinterpret_cast<const char*>(&data[offset]), account_len);
		offset += account_len;
        
		// Read session_token
		uint32_t token_len = *reinterpret_cast<const uint32_t*>(&data[offset]);
		offset += 4;
		if (offset + token_len > data.size()) return false;
		session_token_ = std::string(reinterpret_cast<const char*>(&data[offset]), token_len);
		offset += token_len;
        
		// Read client_version
		if (offset + 4 > data.size()) return false;
		client_version_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
        
		return true;
	}

	auto AuthenticationPacket::from_data(const std::vector<uint8_t>& data)
		-> std::tuple<std::unique_ptr<AuthenticationPacket>, std::optional<std::string>>
	{
		auto packet = std::make_unique<AuthenticationPacket>();
		if (packet->deserialize(data))
		{
			return { std::move(packet), std::nullopt };
		}
		return { nullptr, "Failed to deserialize AuthenticationPacket" };
	}

	auto AuthenticationPacket::clone() const -> std::unique_ptr<GamePacket>
	{
		auto packet = std::make_unique<AuthenticationPacket>();
		packet->account_id_ = account_id_;
		packet->session_token_ = session_token_;
		packet->client_version_ = client_version_;
		return packet;
	}

	// MoveToPacket implementation
	MoveToPacket::MoveToPacket()
		: GamePacket(PacketType::MoveTo)
		, entity_id_(0)
		, movement_speed_(0.0f)
		, movement_type_(0)
	{
	}

	auto MoveToPacket::entity_id() const -> uint64_t
	{
		return entity_id_;
	}

	auto MoveToPacket::destination() const -> Location
	{
		return destination_;
	}

	auto MoveToPacket::movement_speed() const -> float
	{
		return movement_speed_;
	}

	auto MoveToPacket::movement_type() const -> uint8_t
	{
		return movement_type_;
	}

	auto MoveToPacket::set_entity_id(uint64_t id) -> void
	{
		entity_id_ = id;
	}

	auto MoveToPacket::set_destination(const Location& dest) -> void
	{
		destination_ = dest;
	}

	auto MoveToPacket::set_movement_speed(float speed) -> void
	{
		movement_speed_ = speed;
	}

	auto MoveToPacket::set_movement_type(uint8_t type) -> void
	{
		movement_type_ = type;
	}

	auto MoveToPacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		// Add entity_id
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entity_id_), reinterpret_cast<const uint8_t*>(&entity_id_) + 8);
        
		// Add destination (x, y, z as floats)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&destination_.x), reinterpret_cast<const uint8_t*>(&destination_.x) + 4);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&destination_.y), reinterpret_cast<const uint8_t*>(&destination_.y) + 4);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&destination_.z), reinterpret_cast<const uint8_t*>(&destination_.z) + 4);
        
		// Add movement_speed
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&movement_speed_), reinterpret_cast<const uint8_t*>(&movement_speed_) + 4);
        
		// Add movement_type
		data.push_back(movement_type_);
        
		return data;
	}

	auto MoveToPacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 25) return false;
        
		size_t offset = 0;
		entity_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
        
		destination_.x = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
		destination_.y = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
		destination_.z = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
        
		movement_speed_ = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
        
		movement_type_ = data[offset];
        
		return true;
	}

	auto MoveToPacket::from_data(const std::vector<uint8_t>& data)
		-> std::tuple<std::unique_ptr<MoveToPacket>, std::optional<std::string>>
	{
		auto packet = std::make_unique<MoveToPacket>();
        
		if (data.size() < 25) return { nullptr, "Invalid MoveToPacket data size" };
        
		size_t offset = 0;
		packet->entity_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
        
		packet->destination_.x = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
		packet->destination_.y = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
		packet->destination_.z = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
        
		packet->movement_speed_ = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
        
		packet->movement_type_ = data[offset];
        
		return { std::move(packet), std::nullopt };
	}

	auto MoveToPacket::clone() const -> std::unique_ptr<GamePacket>
	{
		auto packet = std::make_unique<MoveToPacket>();
		packet->entity_id_ = entity_id_;
		packet->destination_ = destination_;
		packet->movement_speed_ = movement_speed_;
		packet->movement_type_ = movement_type_;
		return packet;
	}

	// MoveStopPacket implementation
	MoveStopPacket::MoveStopPacket()
		: GamePacket(PacketType::MoveStop)
		, entity_id_(0)
	{
	}

	auto MoveStopPacket::entity_id() const -> uint64_t
	{
		return entity_id_;
	}

	auto MoveStopPacket::stop_location() const -> Location
	{
		return stop_location_;
	}

	auto MoveStopPacket::set_entity_id(uint64_t id) -> void
	{
		entity_id_ = id;
	}

	auto MoveStopPacket::set_stop_location(const Location& loc) -> void
	{
		stop_location_ = loc;
	}

	auto MoveStopPacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		// Add entity_id
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entity_id_), reinterpret_cast<const uint8_t*>(&entity_id_) + 8);
        
		// Add stop_location (x, y, z as floats)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&stop_location_.x), reinterpret_cast<const uint8_t*>(&stop_location_.x) + 4);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&stop_location_.y), reinterpret_cast<const uint8_t*>(&stop_location_.y) + 4);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&stop_location_.z), reinterpret_cast<const uint8_t*>(&stop_location_.z) + 4);
        
		return data;
	}

	auto MoveStopPacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 20) return false;
        
		size_t offset = 0;
		entity_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
        
		stop_location_.x = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
		stop_location_.y = *reinterpret_cast<const float*>(&data[offset]);
		offset += 4;
		stop_location_.z = *reinterpret_cast<const float*>(&data[offset]);
        
		return true;
	}

	// AttackPacket implementation
	AttackPacket::AttackPacket()
		: GamePacket(PacketType::Attack)
		, attacker_id_(0)
		, target_id_(0)
		, attack_type_(0)
		, damage_(0)
		, is_critical_(false)
	{
	}

	auto AttackPacket::attacker_id() const -> uint64_t
	{
		return attacker_id_;
	}

	auto AttackPacket::target_id() const -> uint64_t
	{
		return target_id_;
	}

	auto AttackPacket::attack_type() const -> uint16_t
	{
		return attack_type_;
	}

	auto AttackPacket::damage() const -> uint32_t
	{
		return damage_;
	}

	auto AttackPacket::is_critical() const -> bool
	{
		return is_critical_;
	}

	auto AttackPacket::set_attacker_id(uint64_t id) -> void
	{
		attacker_id_ = id;
	}

	auto AttackPacket::set_target_id(uint64_t id) -> void
	{
		target_id_ = id;
	}

	auto AttackPacket::set_attack_type(uint16_t type) -> void
	{
		attack_type_ = type;
	}

	auto AttackPacket::set_damage(uint32_t dmg) -> void
	{
		damage_ = dmg;
	}

	auto AttackPacket::set_critical(bool crit) -> void
	{
		is_critical_ = crit;
	}

	auto AttackPacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&attacker_id_), reinterpret_cast<const uint8_t*>(&attacker_id_) + 8);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&target_id_), reinterpret_cast<const uint8_t*>(&target_id_) + 8);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&attack_type_), reinterpret_cast<const uint8_t*>(&attack_type_) + 2);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&damage_), reinterpret_cast<const uint8_t*>(&damage_) + 4);
		data.push_back(is_critical_ ? 1 : 0);
        
		return data;
	}

	auto AttackPacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 23) return false;
        
		size_t offset = 0;
		attacker_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
		target_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
		attack_type_ = *reinterpret_cast<const uint16_t*>(&data[offset]);
		offset += 2;
		damage_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
		offset += 4;
		is_critical_ = (data[offset] != 0);
        
		return true;
	}

	// ChatMessagePacket implementation
	ChatMessagePacket::ChatMessagePacket()
		: GamePacket(PacketType::ChatMessage)
		, sender_id_(0)
		, chat_type_(0)
		, channel_id_(0)
	{
	}

	auto ChatMessagePacket::sender_id() const -> uint64_t
	{
		return sender_id_;
	}

	auto ChatMessagePacket::sender_name() const -> std::string
	{
		return sender_name_;
	}

	auto ChatMessagePacket::message() const -> std::string
	{
		return message_;
	}

	auto ChatMessagePacket::chat_type() const -> uint8_t
	{
		return chat_type_;
	}

	auto ChatMessagePacket::channel_id() const -> uint32_t
	{
		return channel_id_;
	}

	auto ChatMessagePacket::set_sender_id(uint64_t id) -> void
	{
		sender_id_ = id;
	}

	auto ChatMessagePacket::set_sender_name(const std::string& name) -> void
	{
		sender_name_ = name;
	}

	auto ChatMessagePacket::set_message(const std::string& msg) -> void
	{
		message_ = msg;
	}

	auto ChatMessagePacket::set_chat_type(uint8_t type) -> void
	{
		chat_type_ = type;
	}

	auto ChatMessagePacket::set_channel_id(uint32_t id) -> void
	{
		channel_id_ = id;
	}

	auto ChatMessagePacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		// Add sender_id
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&sender_id_), reinterpret_cast<const uint8_t*>(&sender_id_) + 8);
        
		// Add sender_name
		auto name_bytes = reinterpret_cast<const uint8_t*>(sender_name_.c_str());
		uint32_t name_len = static_cast<uint32_t>(sender_name_.length());
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&name_len), reinterpret_cast<const uint8_t*>(&name_len) + 4);
		data.insert(data.end(), name_bytes, name_bytes + name_len);
        
		// Add message
		auto msg_bytes = reinterpret_cast<const uint8_t*>(message_.c_str());
		uint32_t msg_len = static_cast<uint32_t>(message_.length());
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&msg_len), reinterpret_cast<const uint8_t*>(&msg_len) + 4);
		data.insert(data.end(), msg_bytes, msg_bytes + msg_len);
        
		// Add chat_type and channel_id
		data.push_back(chat_type_);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&channel_id_), reinterpret_cast<const uint8_t*>(&channel_id_) + 4);
        
		return data;
	}

	auto ChatMessagePacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 21) return false;
        
		size_t offset = 0;
        
		// Read sender_id
		sender_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
        
		// Read sender_name
		uint32_t name_len = *reinterpret_cast<const uint32_t*>(&data[offset]);
		offset += 4;
		if (offset + name_len > data.size()) return false;
		sender_name_ = std::string(reinterpret_cast<const char*>(&data[offset]), name_len);
		offset += name_len;
        
		// Read message
		uint32_t msg_len = *reinterpret_cast<const uint32_t*>(&data[offset]);
		offset += 4;
		if (offset + msg_len > data.size()) return false;
		message_ = std::string(reinterpret_cast<const char*>(&data[offset]), msg_len);
		offset += msg_len;
        
		// Read chat_type and channel_id
		if (offset + 5 > data.size()) return false;
		chat_type_ = data[offset];
		offset += 1;
		channel_id_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
        
		return true;
	}

	// DamagePacket implementation  
	DamagePacket::DamagePacket()
		: GamePacket(PacketType::Damage)
		, target_id_(0)
		, damage_amount_(0)
		, damage_type_(0)
		, source_id_(0)
		, remaining_hp_(0)
	{
	}

	auto DamagePacket::target_id() const -> uint64_t
	{
		return target_id_;
	}

	auto DamagePacket::damage_amount() const -> uint32_t
	{
		return damage_amount_;
	}

	auto DamagePacket::damage_type() const -> uint16_t
	{
		return damage_type_;
	}

	auto DamagePacket::source_id() const -> uint64_t
	{
		return source_id_;
	}

	auto DamagePacket::remaining_hp() const -> uint32_t
	{
		return remaining_hp_;
	}

	auto DamagePacket::set_target_id(uint64_t id) -> void
	{
		target_id_ = id;
	}

	auto DamagePacket::set_damage_amount(uint32_t amount) -> void
	{
		damage_amount_ = amount;
	}

	auto DamagePacket::set_damage_type(uint16_t type) -> void
	{
		damage_type_ = type;
	}

	auto DamagePacket::set_source_id(uint64_t id) -> void
	{
		source_id_ = id;
	}

	auto DamagePacket::set_remaining_hp(uint32_t hp) -> void
	{
		remaining_hp_ = hp;
	}

	auto DamagePacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&target_id_), reinterpret_cast<const uint8_t*>(&target_id_) + 8);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&damage_amount_), reinterpret_cast<const uint8_t*>(&damage_amount_) + 4);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&damage_type_), reinterpret_cast<const uint8_t*>(&damage_type_) + 2);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&source_id_), reinterpret_cast<const uint8_t*>(&source_id_) + 8);
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&remaining_hp_), reinterpret_cast<const uint8_t*>(&remaining_hp_) + 4);
        
		return data;
	}

	auto DamagePacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 26) return false;
        
		size_t offset = 0;
		target_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
		damage_amount_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
		offset += 4;
		damage_type_ = *reinterpret_cast<const uint16_t*>(&data[offset]);
		offset += 2;
		source_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
		remaining_hp_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
        
		return true;
	}

	// EntityUpdatePacket implementation
	EntityUpdatePacket::EntityUpdatePacket()
		: GamePacket(PacketType::EntityUpdate)
		, entity_id_(0)
	{
	}

	auto EntityUpdatePacket::entity_id() const -> uint64_t
	{
		return entity_id_;
	}

	auto EntityUpdatePacket::location() const -> std::optional<Location>
	{
		return location_;
	}

	auto EntityUpdatePacket::health() const -> std::optional<uint32_t>
	{
		return health_;
	}

	auto EntityUpdatePacket::state() const -> std::optional<EntityState>
	{
		return state_;
	}

	auto EntityUpdatePacket::velocity() const -> std::optional<Vector3>
	{
		return velocity_;
	}

	auto EntityUpdatePacket::set_entity_id(uint64_t id) -> void
	{
		entity_id_ = id;
	}

	auto EntityUpdatePacket::set_location(const Location& loc) -> void
	{
		location_ = loc;
	}

	auto EntityUpdatePacket::set_position(const Location& pos) -> void
	{
		location_ = pos;
	}

	auto EntityUpdatePacket::set_rotation(float rotation) -> void
	{
		rotation_ = rotation;
	}

	auto EntityUpdatePacket::set_is_moving(bool moving) -> void
	{
		is_moving_ = moving;
	}

	auto EntityUpdatePacket::set_health(uint32_t health) -> void
	{
		health_ = health;
	}

	auto EntityUpdatePacket::set_state(EntityState state) -> void
	{
		state_ = state;
	}

	auto EntityUpdatePacket::set_velocity(const Vector3& vel) -> void
	{
		velocity_ = vel;
	}

	auto EntityUpdatePacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> data;
        
		// Add entity_id
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entity_id_), reinterpret_cast<const uint8_t*>(&entity_id_) + 8);
        
		// Add optional fields with flags
		uint8_t flags = 0;
		if (location_.has_value()) flags |= 0x01;
		if (health_.has_value()) flags |= 0x02;
		if (state_.has_value()) flags |= 0x04;
		if (velocity_.has_value()) flags |= 0x08;
		if (rotation_.has_value()) flags |= 0x10;
		if (is_moving_.has_value()) flags |= 0x20;
        
		data.push_back(flags);
        
		if (location_.has_value())
		{
			const auto& loc = location_.value();
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&loc.x), reinterpret_cast<const uint8_t*>(&loc.x) + 4);
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&loc.y), reinterpret_cast<const uint8_t*>(&loc.y) + 4);
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&loc.z), reinterpret_cast<const uint8_t*>(&loc.z) + 4);
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&loc.map_id), reinterpret_cast<const uint8_t*>(&loc.map_id) + 4);
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&loc.zone_id), reinterpret_cast<const uint8_t*>(&loc.zone_id) + 4);
		}
        
		if (health_.has_value())
		{
			uint32_t hp = health_.value();
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&hp), reinterpret_cast<const uint8_t*>(&hp) + 4);
		}
        
		if (state_.has_value())
		{
			uint8_t st = static_cast<uint8_t>(state_.value());
			data.push_back(st);
		}
        
		if (velocity_.has_value())
		{
			const auto& vel = velocity_.value();
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&vel.x), reinterpret_cast<const uint8_t*>(&vel.x) + 4);
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&vel.y), reinterpret_cast<const uint8_t*>(&vel.y) + 4);
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&vel.z), reinterpret_cast<const uint8_t*>(&vel.z) + 4);
		}
        
		if (rotation_.has_value())
		{
			float rot = rotation_.value();
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&rot), reinterpret_cast<const uint8_t*>(&rot) + 4);
		}
        
		if (is_moving_.has_value())
		{
			data.push_back(is_moving_.value() ? 1 : 0);
		}
        
		return data;
	}

	auto EntityUpdatePacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 9) return false;
        
		size_t offset = 0;
        
		// Read entity_id
		entity_id_ = *reinterpret_cast<const uint64_t*>(&data[offset]);
		offset += 8;
        
		// Read flags
		uint8_t flags = data[offset];
		offset += 1;
        
		// Read optional fields based on flags
		if (flags & 0x01) // location
		{
			if (offset + 20 > data.size()) return false;
			Location loc;
			loc.x = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
			loc.y = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
			loc.z = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
			loc.map_id = *reinterpret_cast<const uint32_t*>(&data[offset]);
			offset += 4;
			loc.zone_id = *reinterpret_cast<const uint32_t*>(&data[offset]);
			offset += 4;
			location_ = loc;
		}
        
		if (flags & 0x02) // health
		{
			if (offset + 4 > data.size()) return false;
			health_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
			offset += 4;
		}
        
		if (flags & 0x04) // state
		{
			if (offset + 1 > data.size()) return false;
			state_ = static_cast<EntityState>(data[offset]);
			offset += 1;
		}
        
		if (flags & 0x08) // velocity
		{
			if (offset + 12 > data.size()) return false;
			Vector3 vel;
			vel.x = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
			vel.y = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
			vel.z = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
			velocity_ = vel;
		}
        
		if (flags & 0x10) // rotation
		{
			if (offset + 4 > data.size()) return false;
			rotation_ = *reinterpret_cast<const float*>(&data[offset]);
			offset += 4;
		}
        
		if (flags & 0x20) // is_moving
		{
			if (offset + 1 > data.size()) return false;
			is_moving_ = (data[offset] != 0);
			offset += 1;
		}
        
		return true;
	}

	auto EntityUpdatePacket::from_data(const std::vector<uint8_t>& data) -> std::tuple<std::unique_ptr<EntityUpdatePacket>, std::optional<std::string>>
	{
		auto packet = std::make_unique<EntityUpdatePacket>();
		if (!packet->deserialize(data))
		{
			return { nullptr, "Failed to deserialize EntityUpdatePacket" };
		}
		return { std::move(packet), std::nullopt };
	}

	auto EntityUpdatePacket::clone() const -> std::unique_ptr<GamePacket>
	{
		auto packet = std::make_unique<EntityUpdatePacket>();
		packet->entity_id_ = entity_id_;
		packet->location_ = location_;
		packet->health_ = health_;
		packet->state_ = state_;
		packet->velocity_ = velocity_;
		packet->rotation_ = rotation_;
		packet->is_moving_ = is_moving_;
		return packet;
	}

	// GamePacket 기본 구현
	auto GamePacket::clone() const -> std::unique_ptr<GamePacket>
	{
		auto packet = std::make_unique<GamePacket>(type_);
		packet->sequence_number_ = sequence_number_;
		packet->sender_id_ = sender_id_;
		packet->target_id_ = target_id_;
		packet->channel_id_ = channel_id_;
		packet->priority_ = priority_;
		packet->timestamp_ = timestamp_;
		packet->payload_ = payload_;
		packet->custom_data_ = custom_data_;
		packet->metadata = metadata;
		packet->data = data;
		return packet;
	}

	auto GamePacket::serialize() const -> std::vector<uint8_t>
	{
		std::vector<uint8_t> result;
        
		// 패킷 타입 (2 바이트)
		uint16_t type = static_cast<uint16_t>(type_);
		result.push_back(static_cast<uint8_t>(type & 0xFF));
		result.push_back(static_cast<uint8_t>((type >> 8) & 0xFF));
        
		// 데이터 크기 (4 바이트)
		uint32_t size = static_cast<uint32_t>(payload_.size());
		result.push_back(static_cast<uint8_t>(size & 0xFF));
		result.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
		result.push_back(static_cast<uint8_t>((size >> 16) & 0xFF));
		result.push_back(static_cast<uint8_t>((size >> 24) & 0xFF));
        
		// 페이로드 데이터
		result.insert(result.end(), payload_.begin(), payload_.end());
        
		return result;
	}

	auto GamePacket::deserialize(const std::vector<uint8_t>& data) -> bool
	{
		if (data.size() < 6) // 최소 헤더 크기
		{
			return false;
		}
        
		// 패킷 타입 읽기
		uint16_t type = data[0] | (data[1] << 8);
		type_ = static_cast<PacketType>(type);
		packet_type = type_;
        
		// 데이터 크기 읽기
		uint32_t size = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
        
		if (data.size() < 6 + size)
		{
			return false;
		}
        
		// 페이로드 읽기
		payload_.clear();
		payload_.insert(payload_.end(), data.begin() + 6, data.begin() + 6 + size);
        
		return true;
	}
}
