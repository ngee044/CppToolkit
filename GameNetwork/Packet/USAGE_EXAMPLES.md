# Binary Serialization and Packet Fragmentation Usage Examples

## 1. Binary Serialization Example

```cpp
// Create a custom game packet with binary serialization
class MovePacket : public GameNetwork::GamePacket
{
public:
    MovePacket() : GamePacket(PacketType::Move) {}
    
    Location from_location;
    Location to_location;
    float speed;
    
protected:
    auto serialize_to_binary(Serialization::BinarySerializer& serializer) const -> void override
    {
        // Use the BinarySerializable trait for Location
        BinarySerializable<Location>::serialize(serializer, from_location);
        BinarySerializable<Location>::serialize(serializer, to_location);
        serializer.write_float(speed);
    }
    
    auto deserialize_from_binary(Serialization::BinaryDeserializer& deserializer) -> bool override
    {
        auto [from, from_ok] = BinarySerializable<Location>::deserialize(deserializer);
        auto [to, to_ok] = BinarySerializable<Location>::deserialize(deserializer);
        auto [s, s_ok] = deserializer.read_float();
        
        if (!from_ok || !to_ok || !s_ok)
        {
            return false;
        }
        
        from_location = from;
        to_location = to;
        speed = s;
        return true;
    }
};

// Usage
MovePacket packet;
packet.from_location = {100.0f, 50.0f, 0.0f, 1, 1};
packet.to_location = {150.0f, 75.0f, 0.0f, 1, 1};
packet.speed = 5.5f;

// Binary serialization (much more efficient than JSON)
auto binary_data = packet.to_binary();
// Typical size: ~40 bytes vs ~200+ bytes for JSON
```

## 2. Packet Fragmentation Example

```cpp
// Create packet transmitter with 1400 byte MTU
GameNetwork::PacketTransmitter transmitter(1400);
transmitter.enable_compression(true);

// Large packet that needs fragmentation
class InventoryUpdatePacket : public GameNetwork::GamePacket
{
    std::vector<ItemData> items;  // Assume 100+ items
    // ... serialization code
};

InventoryUpdatePacket large_packet;
// ... fill with lots of data (e.g., 5KB)

// Automatically fragments into multiple packets
auto fragments = transmitter.prepare_packet_for_send(large_packet);
// Result: vector of ~4 fragments, each under 1400 bytes

// Send fragments over network
for (const auto& fragment : fragments)
{
    network->send(fragment);
}
```

## 3. Receiving and Reassembling

```cpp
// Receiver side
GameNetwork::PacketTransmitter receiver(1400);

// Process incoming data (might be fragments)
void on_data_received(const std::vector<uint8_t>& data)
{
    auto [packet, is_complete] = receiver.process_received_data(data);
    
    if (is_complete && packet)
    {
        // Complete packet reassembled!
        switch (packet->get_type())
        {
            case PacketType::Move:
            {
                auto move_packet = static_cast<MovePacket*>(packet.get());
                handle_move(move_packet->from_location, move_packet->to_location);
                break;
            }
            // ... other packet types
        }
    }
    // else: fragment received, waiting for more
}
```

## 4. Performance Comparison

```cpp
// JSON Serialization (Old)
ChatMessagePacket json_packet;
json_packet.set_serialization_type(PacketSerializationType::Json);
json_packet.message = "Hello, this is a test message!";
auto json_data = json_packet.serialize();
// Size: ~150 bytes

// Binary Serialization (New)
ChatMessagePacket binary_packet;
binary_packet.set_serialization_type(PacketSerializationType::Binary);
binary_packet.message = "Hello, this is a test message!";
auto binary_data = binary_packet.serialize();
// Size: ~50 bytes (66% smaller!)

// With compression
transmitter.enable_compression(true);
auto compressed_fragments = transmitter.prepare_packet_for_send(binary_packet);
// Further size reduction for larger packets
```

## 5. Integration with Game Server

```cpp
class GameNetworkServer
{
    void initialize()
    {
        // Configure packet handling
        packet_transmitter_ = std::make_unique<PacketTransmitter>(1400);
        packet_transmitter_->set_default_serialization(PacketSerializationType::Binary);
        packet_transmitter_->enable_compression(true);
    }
    
    void broadcast_packet(const GamePacket& packet, const std::vector<SessionId>& recipients)
    {
        // Prepare packet once (with fragmentation if needed)
        auto fragments = packet_transmitter_->prepare_packet_for_send(packet);
        
        // Send to all recipients
        for (const auto& session_id : recipients)
        {
            if (auto session = session_manager_->get_session(session_id))
            {
                for (const auto& fragment : fragments)
                {
                    session->send_data(fragment);
                }
            }
        }
    }
};
```

## Benefits:

1. **Binary Serialization**:
   - 60-80% size reduction compared to JSON
   - Much faster parsing (no string operations)
   - Type-safe serialization with validation
   - Supports efficient varint encoding for integers

2. **Packet Fragmentation**:
   - Handles large packets automatically
   - Works with any MTU size
   - Reliable reassembly with timeout handling
   - Transparent to application layer

3. **Combined Features**:
   - Binary + Compression + Fragmentation = Maximum efficiency
   - Backwards compatible with JSON for migration
   - Built-in checksums for data integrity
   - Protocol versioning for future updates