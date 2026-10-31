#pragma once
#include "canvas/runtime/appearance.hpp"
namespace canvas::runtime {
// Source-profile value DTOs; ordinary SDKs transport owned JSON/arrays only.
struct VectorPathCreateRequest final {
    foundation::ObjectId objectId{};
    semantic::VectorPathGeometry geometry{};
    semantic::Transform2D transform{}; // world placement, converted by the existing owner
};
enum class VectorPathError : std::uint8_t { kNone,kWrongThread,kNotReady,kBusy,kInvalid,kRejected };
struct VectorPathResult final {
    bool applied=false;
    foundation::ObjectId objectId{};
    std::uint64_t generation=0;
    VectorPathError error=VectorPathError::kNotReady;
};
struct VectorPathSnapshot final {
    bool available=false;
    foundation::ObjectId objectId{};
    semantic::VectorPathGeometry geometry{};
    semantic::Transform2D transform{};
    AppearanceValues appearance{};
    std::uint64_t generation=0;
};
}
