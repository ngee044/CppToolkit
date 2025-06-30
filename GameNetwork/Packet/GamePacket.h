#pragma once

#include "../GameNetworkConstants.h"
#include "../Core/Location.h"

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <variant>
#include <optional>
#include <chrono>
#include <unordered_map>

namespace GameNetwork
{
	// Forward declarations
	struct PartyMemberInfo
	{
		uint64_t player_id;
		std::string player_name;
		uint32_t level;
		uint32_t health;
		uint32_t max_health;
		Location location;
	};
    
	struct GuildInfo
	{
		uint64_t guild_id;
		std::string guild_name;
		std::string guild_master_name;
		uint32_t guild_level;
		uint32_t member_count;
		std::string guild_notice;
	};
    
	struct TradeItem
	{
		uint64_t item_id;
		uint32_t item_template_id;
		uint32_t quantity;
		uint8_t slot_index;
	};
    
	struct QuestObjective
	{
		uint32_t objective_id;
		std::string description;
		uint32_t current_progress;
		uint32_t required_progress;
		bool is_completed;
	};
    
	struct QuestRewards
	{
		uint64_t experience;
		uint64_t gold;
		std::vector<std::pair<uint32_t, uint32_t>> items; // item_id, quantity
		std::vector<uint32_t> skills; // skill_ids
	};

	// Packet types
	enum class PacketType : uint16_t
	{
		// System packets (0-999)
		Unknown = 0,
		Heartbeat = 1,
		Authentication = 2,
		Disconnect = 3,
		ServerInfo = 4,
		GameData = 5,
		ServerCommand = 6,
        
		// Movement packets (1000-1999)
		MoveTo = 1000,
		MoveStop = 1001,
		Teleport = 1002,
		ChangeDirection = 1003,
        
		// Combat packets (2000-2999)
		Attack = 2000,
		SkillUse = 2001,
		Damage = 2002,
		Death = 2003,
		Resurrect = 2004,
        
		// Entity packets (3000-3999)
		EntitySpawn = 3000,
		EntityDespawn = 3001,
		EntityUpdate = 3002,
        
		// Chat packets (4000-4999)
		ChatMessage = 4000,
		ChatWhisper = 4001,
		ChatGuild = 4002,
		ChatParty = 4003,
        
		// Item packets (5000-5999)
		ItemPickup = 5000,
		ItemDrop = 5001,
		ItemUse = 5002,
		ItemEquip = 5003,
		ItemUnequip = 5004,
        
		// Party packets (6000-6999)
		PartyInvite = 6000,
		PartyJoin = 6001,
		PartyLeave = 6002,
		PartyKick = 6003,
        
		// Trade packets (7000-7999)
		TradeRequest = 7000,
		TradeAccept = 7001,
		TradeDecline = 7002,
		TradeCancel = 7003,
        
		// Quest packets (8000-8999)
		QuestUpdate = 8000,
		QuestComplete = 8001,
		QuestAccept = 8002,
		QuestAbandon = 8003,
        
		// Session packets (9000-9999)
		SessionCreate = 9000,
		SessionDestroy = 9001,
		Transaction = 9002,
		DamageEvent = 9003,
		StateChange = 9004,
		PositionUpdate = 9005,
		RotationUpdate = 9006,
		AnimationUpdate = 9007,
        
		// Custom packets (10000+)
		Custom = 10000
	};
    
	// Packet header
	struct PacketHeader
	{
		uint16_t type;
		uint16_t flags;
		uint32_t sequence;
		uint32_t size;
		uint32_t checksum;
	};
    
	// Base packet class
	class GamePacket
	{
	public:
		GamePacket();
		GamePacket(PacketType type);
		virtual ~GamePacket();
        
		// Type and metadata
		auto get_type() const -> PacketType;
		auto set_type(PacketType type) -> void;
        
		// Compatibility with legacy code
		auto set_timestamp(uint64_t timestamp) -> void;
		auto get_timestamp() const -> uint64_t;
        
		PacketType packet_type;
		std::vector<uint8_t> data;
        
		auto get_sequence_number() const -> uint64_t;
		auto set_sequence_number(uint64_t seq) -> void;
        
		auto get_sender_id() const -> uint64_t;
		auto set_sender_id(uint64_t id) -> void;
        
		auto get_target_id() const -> uint64_t;
		auto set_target_id(uint64_t id) -> void;
        
		auto get_channel_id() const -> uint32_t { return channel_id_; }
		auto set_channel_id(uint32_t id) -> void { channel_id_ = id; }
        
		auto get_priority() const -> PacketPriority { return priority_; }
		auto set_priority(PacketPriority priority) -> void { priority_ = priority; }
        
		// Clone method for copying packets
		virtual auto clone() const -> std::unique_ptr<GamePacket>;
        
		// Payload
		auto get_payload() const -> const std::vector<uint8_t>&;
		auto set_payload(const std::vector<uint8_t>& data) -> void;
		auto set_payload(std::vector<uint8_t>&& data) -> void;
        
		// Serialization
		virtual auto serialize() const -> std::vector<uint8_t>;
		virtual auto deserialize(const std::vector<uint8_t>& data) -> bool;
		auto to_json() const -> std::string;
		auto from_json(const std::string& json_str) -> bool;
        
		// Custom data
		template<typename T>
		auto set_custom_data(const std::string& key, const T& value) -> void
		{
			custom_data_[key] = std::to_string(value);
		}
        
		auto get_custom_data(const std::string& key) const -> std::optional<std::string>
		{
			auto it = custom_data_.find(key);
			if (it != custom_data_.end())
			{
				return it->second;
			}
			return std::nullopt;
		}
        
	public:
		// Public metadata for easier access
		std::unordered_map<std::string, std::string> metadata;
        
	protected:
		PacketType type_;
		uint64_t sequence_number_;
		uint64_t sender_id_;
		uint64_t target_id_;
		uint32_t channel_id_;
		PacketPriority priority_;
		std::chrono::steady_clock::time_point timestamp_;
		std::vector<uint8_t> payload_;
		std::unordered_map<std::string, std::string> custom_data_;
	};
    
	// Heartbeat packet
	class HeartbeatPacket : public GamePacket
	{
	public:
		HeartbeatPacket();
        
		auto timestamp() const -> uint64_t;
		auto set_timestamp(uint64_t ts) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data) 
			-> std::tuple<std::unique_ptr<HeartbeatPacket>, std::optional<std::string>>;
        
	private:
		uint64_t timestamp_;
	};
    
	// Authentication packet
	class AuthenticationPacket : public GamePacket
	{
	public:
		AuthenticationPacket();
        
		auto account_id() const -> std::string;
		auto session_token() const -> std::string;
		auto client_version() const -> uint32_t;
        
		auto set_account_id(const std::string& id) -> void;
		auto set_session_token(const std::string& token) -> void;
		auto set_client_version(uint32_t version) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		auto clone() const -> std::unique_ptr<GamePacket> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<AuthenticationPacket>, std::optional<std::string>>;
        
	private:
		std::string account_id_;
		std::string session_token_;
		uint32_t client_version_;
	};
    
	// Entity spawn packet
	class EntitySpawnPacket : public GamePacket
	{
	public:
		EntitySpawnPacket();
        
		auto entity_id() const -> uint64_t;
		auto entity_type() const -> EntityType;
		auto location() const -> Location;
		auto name() const -> std::string;
		auto level() const -> uint32_t;
		auto health() const -> uint32_t;
		auto max_health() const -> uint32_t;
        
		auto set_entity_id(uint64_t id) -> void;
		auto set_entity_type(EntityType type) -> void;
		auto set_location(const Location& loc) -> void;
		auto set_name(const std::string& name) -> void;
		auto set_level(uint32_t level) -> void;
		auto set_health(uint32_t health) -> void;
		auto set_max_health(uint32_t max_health) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<EntitySpawnPacket>, std::optional<std::string>>;
        
	private:
		uint64_t entity_id_;
		EntityType entity_type_;
		Location location_;
		std::string name_;
		uint32_t level_;
		uint32_t health_;
		uint32_t max_health_;
	};
    
	// Entity despawn packet
	class EntityDespawnPacket : public GamePacket
	{
	public:
		EntityDespawnPacket();
        
		auto entity_id() const -> uint64_t;
		auto reason() const -> DespawnReason;
        
		auto set_entity_id(uint64_t id) -> void;
		auto set_reason(DespawnReason reason) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<EntityDespawnPacket>, std::optional<std::string>>;
        
	private:
		uint64_t entity_id_;
		DespawnReason reason_;
	};
    
	// Entity update packet
	class EntityUpdatePacket : public GamePacket
	{
	public:
		EntityUpdatePacket();
        
		auto entity_id() const -> uint64_t;
		auto location() const -> std::optional<Location>;
		auto health() const -> std::optional<uint32_t>;
		auto state() const -> std::optional<EntityState>;
		auto velocity() const -> std::optional<Vector3>;
        
		auto set_entity_id(uint64_t id) -> void;
		auto set_location(const Location& loc) -> void;
		auto set_position(const Location& pos) -> void;  // Alias for compatibility
		auto set_rotation(float rotation) -> void;
		auto set_is_moving(bool moving) -> void;
		auto set_health(uint32_t health) -> void;
		auto set_state(EntityState state) -> void;
		auto set_velocity(const Vector3& vel) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		auto clone() const -> std::unique_ptr<GamePacket> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<EntityUpdatePacket>, std::optional<std::string>>;
        
	private:
		uint64_t entity_id_;
		std::optional<Location> location_;
		std::optional<uint32_t> health_;
		std::optional<EntityState> state_;
		std::optional<Vector3> velocity_;
		std::optional<float> rotation_;
		std::optional<bool> is_moving_;
	};
    
	// Movement packet - MoveTo
	class MoveToPacket : public GamePacket
	{
	public:
		MoveToPacket();
        
		auto entity_id() const -> uint64_t;
		auto destination() const -> Location;
		auto movement_speed() const -> float;
		auto movement_type() const -> uint8_t; // walk, run, etc.
        
		auto set_entity_id(uint64_t id) -> void;
		auto set_destination(const Location& dest) -> void;
		auto set_movement_speed(float speed) -> void;
		auto set_movement_type(uint8_t type) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		auto clone() const -> std::unique_ptr<GamePacket> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<MoveToPacket>, std::optional<std::string>>;
        
	private:
		uint64_t entity_id_;
		Location destination_;
		float movement_speed_;
		uint8_t movement_type_;
	};
    
	// Movement packet - MoveStop
	class MoveStopPacket : public GamePacket
	{
	public:
		MoveStopPacket();
        
		auto entity_id() const -> uint64_t;
		auto stop_location() const -> Location;
        
		auto set_entity_id(uint64_t id) -> void;
		auto set_stop_location(const Location& loc) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<MoveStopPacket>, std::optional<std::string>>;
        
	private:
		uint64_t entity_id_;
		Location stop_location_;
	};
    
	// Movement packet - Teleport
	class TeleportPacket : public GamePacket
	{
	public:
		TeleportPacket();
        
		auto entity_id() const -> uint64_t;
		auto target_location() const -> Location;
		auto teleport_type() const -> uint8_t; // instant, portal, recall, etc.
        
		auto set_entity_id(uint64_t id) -> void;
		auto set_target_location(const Location& loc) -> void;
		auto set_teleport_type(uint8_t type) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<TeleportPacket>, std::optional<std::string>>;
        
	private:
		uint64_t entity_id_;
		Location target_location_;
		uint8_t teleport_type_;
	};
    
	// Combat packet - Attack
	class AttackPacket : public GamePacket
	{
	public:
		AttackPacket();
        
		auto attacker_id() const -> uint64_t;
		auto target_id() const -> uint64_t;
		auto attack_type() const -> uint16_t;
		auto damage() const -> uint32_t;
		auto is_critical() const -> bool;
        
		auto set_attacker_id(uint64_t id) -> void;
		auto set_target_id(uint64_t id) -> void;
		auto set_attack_type(uint16_t type) -> void;
		auto set_damage(uint32_t dmg) -> void;
		auto set_critical(bool crit) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<AttackPacket>, std::optional<std::string>>;
        
	private:
		uint64_t attacker_id_;
		uint64_t target_id_;
		uint16_t attack_type_;
		uint32_t damage_;
		bool is_critical_;
	};
    
	// Combat packet - SkillUse
	class SkillUsePacket : public GamePacket
	{
	public:
		SkillUsePacket();
        
		auto caster_id() const -> uint64_t;
		auto skill_id() const -> uint32_t;
		auto target_id() const -> uint64_t;
		auto target_location() const -> Location;
		auto cast_time() const -> uint32_t;
        
		auto set_caster_id(uint64_t id) -> void;
		auto set_skill_id(uint32_t id) -> void;
		auto set_target_id(uint64_t id) -> void;
		auto set_target_location(const Location& loc) -> void;
		auto set_cast_time(uint32_t time) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<SkillUsePacket>, std::optional<std::string>>;
        
	private:
		uint64_t caster_id_;
		uint32_t skill_id_;
		uint64_t target_id_;
		Location target_location_;
		uint32_t cast_time_;
	};
    
	// Combat packet - Damage
	class DamagePacket : public GamePacket
	{
	public:
		DamagePacket();
        
		auto target_id() const -> uint64_t;
		auto damage_amount() const -> uint32_t;
		auto damage_type() const -> uint16_t;
		auto source_id() const -> uint64_t;
		auto remaining_hp() const -> uint32_t;
        
		auto set_target_id(uint64_t id) -> void;
		auto set_damage_amount(uint32_t amount) -> void;
		auto set_damage_type(uint16_t type) -> void;
		auto set_source_id(uint64_t id) -> void;
		auto set_remaining_hp(uint32_t hp) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<DamagePacket>, std::optional<std::string>>;
        
	private:
		uint64_t target_id_;
		uint32_t damage_amount_;
		uint16_t damage_type_;
		uint64_t source_id_;
		uint32_t remaining_hp_;
	};
    
	// Chat packet - ChatMessage
	class ChatMessagePacket : public GamePacket
	{
	public:
		ChatMessagePacket();
        
		auto sender_id() const -> uint64_t;
		auto sender_name() const -> std::string;
		auto message() const -> std::string;
		auto chat_type() const -> uint8_t; // general, party, guild, etc.
		auto channel_id() const -> uint32_t;
        
		auto set_sender_id(uint64_t id) -> void;
		auto set_sender_name(const std::string& name) -> void;
		auto set_message(const std::string& msg) -> void;
		auto set_chat_type(uint8_t type) -> void;
		auto set_channel_id(uint32_t id) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		auto deserialize(const std::vector<uint8_t>& data) -> bool override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<ChatMessagePacket>, std::optional<std::string>>;
        
	private:
		uint64_t sender_id_;
		std::string sender_name_;
		std::string message_;
		uint8_t chat_type_;
		uint32_t channel_id_;
	};
    
	// Chat packet - ChatWhisper
	class ChatWhisperPacket : public GamePacket
	{
	public:
		ChatWhisperPacket();
        
		auto sender_id() const -> uint64_t;
		auto sender_name() const -> std::string;
		auto recipient_name() const -> std::string;
		auto message() const -> std::string;
        
		auto set_sender_id(uint64_t id) -> void;
		auto set_sender_name(const std::string& name) -> void;
		auto set_recipient_name(const std::string& name) -> void;
		auto set_message(const std::string& msg) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<ChatWhisperPacket>, std::optional<std::string>>;
        
	private:
		uint64_t sender_id_;
		std::string sender_name_;
		std::string recipient_name_;
		std::string message_;
	};
    
	// Party packet - PartyInvite
	class PartyInvitePacket : public GamePacket
	{
	public:
		PartyInvitePacket();
        
		auto inviter_id() const -> uint64_t;
		auto inviter_name() const -> std::string;
		auto invited_player_id() const -> uint64_t;
		auto party_id() const -> uint64_t;
        
		auto set_inviter_id(uint64_t id) -> void;
		auto set_inviter_name(const std::string& name) -> void;
		auto set_invited_player_id(uint64_t id) -> void;
		auto set_party_id(uint64_t id) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<PartyInvitePacket>, std::optional<std::string>>;
        
	private:
		uint64_t inviter_id_;
		std::string inviter_name_;
		uint64_t invited_player_id_;
		uint64_t party_id_;
	};
    
	// Party packet - PartyJoin
	class PartyJoinPacket : public GamePacket
	{
	public:
		PartyJoinPacket();
        
		auto player_id() const -> uint64_t;
		auto party_id() const -> uint64_t;
		auto member_info() const -> std::vector<PartyMemberInfo>;
        
		auto set_player_id(uint64_t id) -> void;
		auto set_party_id(uint64_t id) -> void;
		auto add_member(const PartyMemberInfo& member) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<PartyJoinPacket>, std::optional<std::string>>;
        
	private:
		uint64_t player_id_;
		uint64_t party_id_;
		std::vector<PartyMemberInfo> members_;
	};
    
	// Party packet - PartyLeave
	class PartyLeavePacket : public GamePacket
	{
	public:
		PartyLeavePacket();
        
		auto player_id() const -> uint64_t;
		auto party_id() const -> uint64_t;
		auto reason() const -> uint8_t; // kicked, left, disconnected
        
		auto set_player_id(uint64_t id) -> void;
		auto set_party_id(uint64_t id) -> void;
		auto set_reason(uint8_t reason) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<PartyLeavePacket>, std::optional<std::string>>;
        
	private:
		uint64_t player_id_;
		uint64_t party_id_;
		uint8_t reason_;
	};
    
	// Guild packet - GuildInvite
	class GuildInvitePacket : public GamePacket
	{
	public:
		GuildInvitePacket();
        
		auto inviter_id() const -> uint64_t;
		auto inviter_name() const -> std::string;
		auto invited_player_id() const -> uint64_t;
		auto guild_id() const -> uint64_t;
		auto guild_name() const -> std::string;
        
		auto set_inviter_id(uint64_t id) -> void;
		auto set_inviter_name(const std::string& name) -> void;
		auto set_invited_player_id(uint64_t id) -> void;
		auto set_guild_id(uint64_t id) -> void;
		auto set_guild_name(const std::string& name) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<GuildInvitePacket>, std::optional<std::string>>;
        
	private:
		uint64_t inviter_id_;
		std::string inviter_name_;
		uint64_t invited_player_id_;
		uint64_t guild_id_;
		std::string guild_name_;
	};
    
	// Guild packet - GuildJoin
	class GuildJoinPacket : public GamePacket
	{
	public:
		GuildJoinPacket();
        
		auto player_id() const -> uint64_t;
		auto player_name() const -> std::string;
		auto guild_id() const -> uint64_t;
		auto guild_info() const -> GuildInfo;
        
		auto set_player_id(uint64_t id) -> void;
		auto set_player_name(const std::string& name) -> void;
		auto set_guild_id(uint64_t id) -> void;
		auto set_guild_info(const GuildInfo& info) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<GuildJoinPacket>, std::optional<std::string>>;
        
	private:
		uint64_t player_id_;
		std::string player_name_;
		uint64_t guild_id_;
		GuildInfo guild_info_;
	};
    
	// Guild packet - GuildLeave
	class GuildLeavePacket : public GamePacket
	{
	public:
		GuildLeavePacket();
        
		auto player_id() const -> uint64_t;
		auto guild_id() const -> uint64_t;
		auto reason() const -> uint8_t; // kicked, left, disbanded
        
		auto set_player_id(uint64_t id) -> void;
		auto set_guild_id(uint64_t id) -> void;
		auto set_reason(uint8_t reason) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<GuildLeavePacket>, std::optional<std::string>>;
        
	private:
		uint64_t player_id_;
		uint64_t guild_id_;
		uint8_t reason_;
	};
    
	// Trade packet - TradeRequest
	class TradeRequestPacket : public GamePacket
	{
	public:
		TradeRequestPacket();
        
		auto requester_id() const -> uint64_t;
		auto requester_name() const -> std::string;
		auto target_id() const -> uint64_t;
        
		auto set_requester_id(uint64_t id) -> void;
		auto set_requester_name(const std::string& name) -> void;
		auto set_target_id(uint64_t id) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<TradeRequestPacket>, std::optional<std::string>>;
        
	private:
		uint64_t requester_id_;
		std::string requester_name_;
		uint64_t target_id_;
	};
    
	// Trade packet - TradeStart
	class TradeStartPacket : public GamePacket
	{
	public:
		TradeStartPacket();
        
		auto player1_id() const -> uint64_t;
		auto player2_id() const -> uint64_t;
		auto trade_id() const -> uint64_t;
        
		auto set_player1_id(uint64_t id) -> void;
		auto set_player2_id(uint64_t id) -> void;
		auto set_trade_id(uint64_t id) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<TradeStartPacket>, std::optional<std::string>>;
        
	private:
		uint64_t player1_id_;
		uint64_t player2_id_;
		uint64_t trade_id_;
	};
    
	// Trade packet - TradeUpdate
	class TradeUpdatePacket : public GamePacket
	{
	public:
		TradeUpdatePacket();
        
		auto trade_id() const -> uint64_t;
		auto player_id() const -> uint64_t;
		auto items() const -> std::vector<TradeItem>;
		auto gold() const -> uint64_t;
		auto is_locked() const -> bool;
        
		auto set_trade_id(uint64_t id) -> void;
		auto set_player_id(uint64_t id) -> void;
		auto add_item(const TradeItem& item) -> void;
		auto set_gold(uint64_t gold) -> void;
		auto set_locked(bool locked) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<TradeUpdatePacket>, std::optional<std::string>>;
        
	private:
		uint64_t trade_id_;
		uint64_t player_id_;
		std::vector<TradeItem> items_;
		uint64_t gold_;
		bool is_locked_;
	};
    
	// Trade packet - TradeComplete
	class TradeCompletePacket : public GamePacket
	{
	public:
		TradeCompletePacket();
        
		auto trade_id() const -> uint64_t;
		auto success() const -> bool;
		auto reason() const -> std::string;
        
		auto set_trade_id(uint64_t id) -> void;
		auto set_success(bool success) -> void;
		auto set_reason(const std::string& reason) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<TradeCompletePacket>, std::optional<std::string>>;
        
	private:
		uint64_t trade_id_;
		bool success_;
		std::string reason_;
	};
    
	// Quest packet - QuestUpdate
	class QuestUpdatePacket : public GamePacket
	{
	public:
		QuestUpdatePacket();
        
		auto player_id() const -> uint64_t;
		auto quest_id() const -> uint32_t;
		auto quest_state() const -> uint8_t; // accepted, completed, failed, abandoned
		auto objectives() const -> std::vector<QuestObjective>;
        
		auto set_player_id(uint64_t id) -> void;
		auto set_quest_id(uint32_t id) -> void;
		auto set_quest_state(uint8_t state) -> void;
		auto add_objective(const QuestObjective& objective) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<QuestUpdatePacket>, std::optional<std::string>>;
        
	private:
		uint64_t player_id_;
		uint32_t quest_id_;
		uint8_t quest_state_;
		std::vector<QuestObjective> objectives_;
	};
    
	// Quest packet - QuestComplete
	class QuestCompletePacket : public GamePacket
	{
	public:
		QuestCompletePacket();
        
		auto player_id() const -> uint64_t;
		auto quest_id() const -> uint32_t;
		auto rewards() const -> QuestRewards;
        
		auto set_player_id(uint64_t id) -> void;
		auto set_quest_id(uint32_t id) -> void;
		auto set_rewards(const QuestRewards& rewards) -> void;
        
		auto serialize() const -> std::vector<uint8_t> override;
		static auto from_data(const std::vector<uint8_t>& data)
			-> std::tuple<std::unique_ptr<QuestCompletePacket>, std::optional<std::string>>;
        
	private:
		uint64_t player_id_;
		uint32_t quest_id_;
		QuestRewards rewards_;
	};
}
