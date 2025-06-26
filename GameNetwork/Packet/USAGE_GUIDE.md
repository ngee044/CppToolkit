# Binary Serialization and Packet Fragmentation Usage

## 1. Binary Serialization Example

```cpp
#include "GameNetwork/Packet/GamePacketExamples.h"

using namespace GameNetwork;

// Create and serialize a move packet
MovePacket move_packet;
move_packet.entity_id_ = 12345;
move_packet.from_location_ = {100.0f, 50.0f, 0.0f, 1, 1};
move_packet.to_location_ = {150.0f, 75.0f, 0.0f, 1, 1};
move_packet.speed_ = 5.5f;
move_packet.movement_flags_ = 0x01;  // Walking

// Binary serialization
std::vector<uint8_t> binary_data = move_packet.serialize();
// Size: ~49 bytes (compared to 200+ bytes for JSON)

// Deserialize
MovePacket received_packet;
if (received_packet.deserialize(binary_data))
{
    Logger::info("Move packet received: entity " + 
                 std::to_string(received_packet.entity_id_));
}
```

## 2. Packet Fragmentation Example

```cpp
#include "GameNetwork/Packet/PacketManager.h"

// Create packet manager with 1400 byte MTU
PacketManager packet_manager(1400);

// Large chat message that needs fragmentation
ChatPacket chat_packet;
chat_packet.sender_id_ = 99999;
chat_packet.sender_name_ = "Player123";
chat_packet.message_ = std::string(5000, 'A');  // 5KB message
chat_packet.channel_ = 1;  // World chat

// Automatically fragment if needed
auto [success, fragments] = packet_manager.prepare_packet_for_send(chat_packet);

if (success)
{
    Logger::info("Packet fragmented into " + 
                 std::to_string(fragments.size()) + " pieces");
    
    // Send each fragment
    for (const auto& fragment : fragments)
    {
        network->send(fragment);
    }
}
```

## 3. Receiving and Reassembling

```cpp
// Receiver side
PacketManager receiver_manager(1400);

void on_data_received(const std::vector<uint8_t>& data)
{
    auto [complete, packet] = receiver_manager.process_received_data(data);
    
    if (complete && packet)
    {
        // Complete packet reassembled!
        switch (packet->get_type())
        {
            case PacketType::MoveTo:
            {
                auto* move_packet = static_cast<MovePacket*>(packet.get());
                handle_movement(move_packet->entity_id_, 
                              move_packet->to_location_);
                break;
            }
            
            case PacketType::ChatMessage:
            {
                auto* chat_packet = static_cast<ChatPacket*>(packet.get());
                display_chat(chat_packet->sender_name_, 
                           chat_packet->message_);
                break;
            }
        }
    }
    // else: fragment received, waiting for more
}

// Periodic cleanup
void maintenance_tick()
{
    size_t cleaned = receiver_manager.cleanup_timeout_fragments();
    if (cleaned > 0)
    {
        Logger::debug("Cleaned up " + std::to_string(cleaned) + 
                     " timed out messages");
    }
}
```

## 4. Performance Comparison

### JSON vs Binary Serialization

| Packet Type | JSON Size | Binary Size | Reduction |
|------------|-----------|-------------|-----------|
| MovePacket | ~200 bytes | 49 bytes | 75% |
| ChatPacket (100 char) | ~180 bytes | 120 bytes | 33% |
| InventoryUpdate (50 items) | ~4KB | ~1KB | 75% |

### Benefits:
- **Smaller size**: 33-75% size reduction
- **Faster parsing**: No string parsing needed
- **Type safety**: Compile-time type checking
- **Automatic fragmentation**: Handles large packets transparently
- **Reassembly with timeout**: Cleans up incomplete messages

## 5. Custom Packet Implementation

```cpp
class InventoryUpdatePacket : public BinaryGamePacket
{
public:
    InventoryUpdatePacket() : BinaryGamePacket(PacketType::InventoryUpdate) {}
    
    struct Item
    {
        uint64_t item_id;
        uint32_t template_id;
        uint16_t quantity;
        uint8_t slot;
    };
    
    std::vector<Item> items_;
    
protected:
    auto write_to_buffer(BinaryBuffer& buffer) const -> void override
    {
        buffer.write_uint32(static_cast<uint32_t>(items_.size()));
        
        for (const auto& item : items_)
        {
            buffer.write_uint64(item.item_id);
            buffer.write_uint32(item.template_id);
            buffer.write_uint16(item.quantity);
            buffer.write_uint8(item.slot);
        }
    }
    
    auto read_from_buffer(BinaryBuffer& buffer) -> bool override
    {
        auto [count_success, count] = buffer.read_uint32();
        if (!count_success)
        {
            return false;
        }
        
        items_.clear();
        items_.reserve(count);
        
        for (uint32_t i = 0; i < count; ++i)
        {
            Item item;
            auto [id_ok, id] = buffer.read_uint64();
            auto [tid_ok, tid] = buffer.read_uint32();
            auto [qty_ok, qty] = buffer.read_uint16();
            auto [slot_ok, slot] = buffer.read_uint8();
            
            if (!id_ok || !tid_ok || !qty_ok || !slot_ok)
            {
                return false;
            }
            
            item.item_id = id;
            item.template_id = tid;
            item.quantity = qty;
            item.slot = slot;
            
            items_.push_back(item);
        }
        
        return true;
    }
};
```