#pragma once

#include <glm/glm.hpp>

struct Location {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float pitch{0.0f};
    float yaw{0.0f};
    float roll{0.0f};
    uint32_t map_id{0};
    uint32_t zone_id{0};

    Location() = default;
    
    Location(float x, float y, float z, float pitch = 0.0f, float yaw = 0.0f, float roll = 0.0f)
        : position(x, y, z), x(x), y(y), z(z), pitch(pitch), yaw(yaw), roll(roll) {}
    
    Location(const glm::vec3& pos, float pitch = 0.0f, float yaw = 0.0f, float roll = 0.0f)
        : position(pos), x(pos.x), y(pos.y), z(pos.z), pitch(pitch), yaw(yaw), roll(roll) {}
};

// Velocity is represented as a 3D vector
using Velocity = glm::vec3;
