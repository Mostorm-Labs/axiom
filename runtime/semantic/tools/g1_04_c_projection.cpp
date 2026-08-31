#include "g1_04_c_projection.hpp"

#include <iomanip>
#include <sstream>
#include <algorithm>

namespace canvas::verification::g1_04_c {
namespace {
using namespace canvas::semantic;

std::string idHex(const ObjectId& id) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : id.bytes) out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}
}

std::string projectObjects(const std::vector<ObjectRecord>& objects) {
    std::vector<ObjectRecord> sorted = objects;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::ostringstream out;
    for (const auto& object : sorted) {
        out << idHex(object.id) << ':' << static_cast<unsigned>(object.kind) << ':' << object.kind_version << ':';
        if (object.placement.parent_id.has_value()) out << idHex(*object.placement.parent_id); else out << '-';
        out << ':';
        for (const auto byte : object.placement.order_key.bytes()) out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(byte);
        out << '\n';
    }
    return out.str();
}

std::string projectStore(const canvas::semantic::ObjectStore& store) {
    return projectObjects(store.allObjects());
}

} // namespace canvas::verification::g1_04_c
