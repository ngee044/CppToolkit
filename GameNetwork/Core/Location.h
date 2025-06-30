#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace GameNetwork {

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

		// Operators
		bool operator==(const Location& other) const {
			return position == other.position && pitch == other.pitch && yaw == other.yaw && roll == other.roll && map_id == other.map_id && zone_id == other.zone_id;
		}

		bool operator!=(const Location& other) const {
			return !(*this == other);
		}

		// Distance calculation
		float distance_to(const Location& other) const {
			return glm::distance(position, other.position);
		}

		// Distance calculation (2D)
		float distance_to_2d(const Location& other) const {
			glm::vec2 pos1(position.x, position.y);
			glm::vec2 pos2(other.position.x, other.position.y);
			return glm::distance(pos1, pos2);
		}
	};

}
