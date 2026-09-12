#include "canvas/scene/runtime_scene_projection.hpp"

#include "canvas/scene/bounds_system.hpp"

#include <algorithm>
#include <sstream>
#include <type_traits>

namespace canvas::scene {
namespace {
std::string geometryText(const semantic::ObjectRecord& r) {
    std::ostringstream out;
    out.precision(17);
    out << static_cast<unsigned>(r.kind) << ':' << r.transform.a << ',' << r.transform.b << ','
        << r.transform.c << ',' << r.transform.d << ',' << r.transform.tx << ',' << r.transform.ty;
    std::visit([&](const auto& c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, semantic::ShapeContent>) {
            out << ":shape:" << c.shape_kind << ':' << c.width << ':' << c.height;
        } else if constexpr (std::is_same_v<T, semantic::ImageContent>) {
            out << ":image:" << c.resource_id.value.bytes[0] << ':' << c.width << ':' << c.height;
        } else if constexpr (std::is_same_v<T, semantic::VectorPathContent>) {
            out << ":path:" << static_cast<unsigned>(c.geometry.fill_rule) << ':' << c.geometry.commands.size();
            for (const auto& command : c.geometry.commands) {
                std::visit([&](const auto& value) { out << ':' << sizeof(value); }, command);
            }
        } else if constexpr (std::is_same_v<T, semantic::RichTextContent>) {
            out << ":text:" << c.document.paragraphs.size();
            for (const auto& paragraph : c.document.paragraphs) for (const auto& run : paragraph.runs) out << ':' << run.text;
        } else if constexpr (std::is_same_v<T, semantic::VectorStrokeContent>) {
            out << ":vstroke:" << c.stroke.deterministic_seed;
            if (const auto* data = std::get_if<semantic::VectorStrokeData>(&c.stroke.data)) out << ':' << data->samples.size();
        } else if constexpr (std::is_same_v<T, semantic::DabStrokeContent>) {
            out << ":dstroke:" << c.stroke.deterministic_seed;
            if (const auto* data = std::get_if<semantic::DabStrokeData>(&c.stroke.data)) out << ':' << data->dabs.size();
        } else if constexpr (std::is_same_v<T, semantic::ConnectorContent>) {
            out << ":connector:" << static_cast<unsigned>(c.routing);
        } else if constexpr (std::is_same_v<T, semantic::StickyContent>) {
            out << ":sticky:" << c.width << ':' << c.height;
        } else {
            out << ":group";
        }
    }, r.content);
    return out.str();
}
std::vector<semantic::ObjectId> dependencies(const semantic::ObjectRecord& r) {
    std::vector<semantic::ObjectId> result;
    if (r.placement.parent_id) result.push_back(*r.placement.parent_id);
    if (const auto* image = std::get_if<semantic::ImageContent>(&r.content)) {
        if (!image->resource_id.value.isZero()) result.push_back(image->resource_id.value);
    }
    if (const auto* stroke = std::get_if<semantic::VectorStrokeContent>(&r.content)) {
        if (stroke->stroke.brush.texture_resource_id) result.push_back(stroke->stroke.brush.texture_resource_id->value);
    }
    if (const auto* stroke = std::get_if<semantic::DabStrokeContent>(&r.content)) {
        if (stroke->stroke.brush.texture_resource_id) result.push_back(stroke->stroke.brush.texture_resource_id->value);
    }
    if (const auto* text = std::get_if<semantic::RichTextContent>(&r.content)) {
        for (const auto& paragraph : text->document.paragraphs) for (const auto& run : paragraph.runs)
            if (run.style.font_resource_id) result.push_back(run.style.font_resource_id->value);
    }
    if (const auto* connector = std::get_if<semantic::ConnectorContent>(&r.content)) {
        for (const auto* endpoint : {&connector->start, &connector->end}) {
            if (const auto* attached = std::get_if<semantic::AttachedEndpoint>(&endpoint->value)) {
                result.push_back(attached->target_object_id);
            }
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
}

const InspectionRecord* RuntimeSceneProjection::find(semantic::ObjectId id) const noexcept {
    const auto it = std::find_if(records.begin(), records.end(), [id](const auto& r) { return r.objectId == id; });
    return it == records.end() ? nullptr : &*it;
}

foundation::Result<RuntimeSceneProjection> projectSemanticScene(const semantic::SemanticReadView& view) {
    RuntimeSceneProjection out;
    out.generation = view.generation();
    auto source = view.allObjects();
    out.records.reserve(source.size());
    for (const auto& record : source) {
        if (record.id.isZero() || !semantic::isKnownObjectKind(record.kind)) {
            return foundation::Result<RuntimeSceneProjection>::failure({foundation::ErrorCode::kInvalidRecord, "invalid semantic object"});
        }
        const auto bounds = computeBounds(record);
        out.records.push_back(InspectionRecord{record.id, record.kind, record.kind_version,
            record.placement, record.transform, record.properties, record.content, record.erase_masks,
            bounds.geometry, bounds.visual, bounds.world, geometryText(record), dependencies(record)});
    }
    std::sort(out.records.begin(), out.records.end(), [](const auto& a, const auto& b) {
        if (a.placement.order_key != b.placement.order_key) return a.placement.order_key < b.placement.order_key;
        return a.objectId < b.objectId;
    });
    return foundation::Result<RuntimeSceneProjection>::success(std::move(out));
}
} // namespace canvas::scene
