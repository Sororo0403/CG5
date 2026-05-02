#pragma once
#include <cstdint>
#include <functional>

struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool IsValid() const { return index != 0 && generation != 0; }
};

constexpr Entity kInvalidEntity{};

inline bool operator==(Entity lhs, Entity rhs) {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(Entity lhs, Entity rhs) { return !(lhs == rhs); }

inline bool operator<(Entity lhs, Entity rhs) {
    if (lhs.index != rhs.index) {
        return lhs.index < rhs.index;
    }
    return lhs.generation < rhs.generation;
}

inline bool operator>(Entity lhs, Entity rhs) { return rhs < lhs; }

inline bool operator<=(Entity lhs, Entity rhs) { return !(rhs < lhs); }

inline bool operator>=(Entity lhs, Entity rhs) { return !(lhs < rhs); }

namespace std {

template <>
struct hash<Entity> {
    size_t operator()(Entity entity) const noexcept {
        return (static_cast<size_t>(entity.index) << 32) ^
               static_cast<size_t>(entity.generation);
    }
};

} // namespace std
