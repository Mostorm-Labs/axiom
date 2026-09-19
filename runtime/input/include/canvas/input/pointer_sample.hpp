#pragma once
#include <cstdint>
namespace canvas::input { struct PointerSample final { std::uint64_t sequence=0; std::uint64_t timestampNs=0; float x=0; float y=0; float pressure=0; bool predicted=false; }; }
