#include "g1_04_c_projection.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace canvas::verification::g1_04_c {
namespace {
using namespace canvas::semantic;

std::string idHex(const ObjectId& id) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : id.bytes) out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}

std::string orderKeyHex(const OrderKey& orderKey) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : orderKey.bytes()) out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}
} // namespace

ObjectProjection projectObjects(const std::vector<ObjectRecord>& objects) {
    std::vector<ObjectRecord> sorted = objects;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    ObjectProjection projection;
    projection.objects.reserve(sorted.size());
    for (const auto& object : sorted) {
        projection.objects.push_back(ObjectProjectionRecord{
            idHex(object.id),
            static_cast<std::uint32_t>(object.kind),
            object.kind_version,
            object.placement.parent_id.has_value()
                ? std::optional<std::string>(idHex(*object.placement.parent_id)) : std::nullopt,
            orderKeyHex(object.placement.order_key),
        });
    }
    return projection;
}

ObjectProjection projectStore(const canvas::semantic::ObjectStore& store) {
    return projectObjects(store.allObjects());
}

PlanProjection projectPlan(const canvas::semantic::PreparedApplyPlan& plan) {
    PlanProjection projection;
    projection.creates = projectObjects(plan.creates);
    projection.replacements = projectObjects(plan.replacements);
    for (const auto& id : plan.deletes) projection.deletes.push_back(idHex(id));
    if (plan.delete_closure.has_value()) {
        std::vector<std::string> closure;
        for (const auto& id : plan.delete_closure->final_delete_set) closure.push_back(idHex(id));
        projection.deleteClosure = std::move(closure);
    }
    return projection;
}

} // namespace canvas::verification::g1_04_c
