#include "canvas/semantic/snapshot.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace canvas;
using nlohmann::json;
namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
semantic::SemanticSnapshot read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "snapshot absent");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    auto decoded = semantic::SnapshotCodec::decode(bytes, semantic::SnapshotCapability::kAllWithCompactStrokeV1);
    require(decoded.ok(), "snapshot decode failed");
    return std::move(*decoded.snapshot);
}
std::string hex(foundation::ObjectId id) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (auto byte : id.bytes) { out += digits[byte >> 4U]; out += digits[byte & 15U]; }
    return out;
}
const semantic::ObjectRecord* find(const semantic::SemanticSnapshot& snapshot, const std::string& id) {
    const auto it = std::find_if(snapshot.objects.begin(), snapshot.objects.end(), [&](const auto& object) {
        return hex(object.id) == id;
    });
    return it == snapshot.objects.end() ? nullptr : &*it;
}
bool pose(const semantic::Transform2D& transform, const json& matrix) {
    const double actual[]{transform.a, transform.b, transform.c, transform.d, transform.tx, transform.ty};
    if (matrix.size() != 6) return false;
    for (unsigned i = 0; i < 6; ++i) if (std::abs(actual[i] - matrix[i].get<double>()) > 1e-7) return false;
    return true;
}
void color(const semantic::ColorValue& actual, const json& expected) {
    const float values[]{actual.r, actual.g, actual.b, actual.a};
    for (unsigned i = 0; i < 4; ++i)
        require(std::abs(std::round(values[i] * 255) - expected[i].get<int>()) <= 1, "canonical material mismatch");
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected qualified row path");
        const std::filesystem::path rowPath(argv[1]);
        std::ifstream rowInput(rowPath); const auto row = json::parse(rowInput);
        const auto directory = rowPath.parent_path();
        const auto initial = read(directory / "initial.axsnap"), final = read(directory / "final.axsnap"),
                   undo = read(directory / "undo.axsnap"), redo = read(directory / "redo.axsnap");
        const auto kind = row.at("kind").get<std::string>();
        const auto targetId = row.at("plan").at("primary").get<std::string>();
        const auto* target = find(final, targetId); const auto* undone = find(undo, targetId);
        require(target && undone, "target identity absent");
        require(initial.document_id == final.document_id && final.document_id == redo.document_id, "foreign document");
        require(final.objects == redo.objects && final.document_digest == redo.document_digest, "Redo does not restore canonical values");
        require(initial.manifest == final.manifest && final.manifest == undo.manifest && undo.manifest == redo.manifest,
                "resource manifest changed");
        require(initial.background == final.background, "background changed");
        const bool created = kind != "image";
        require(final.objects.size() == initial.objects.size() + unsigned(created), "create cardinality");
        require(bool(find(initial, targetId)) != created, "wrong target create identity");
        require(pose(target->transform, row.at("afterSelection").at("transform")), "final pose mismatch");
        require(pose(undone->transform, row.at("plan").at("initialTransform")), "Undo pose mismatch");
        auto restored = *target; restored.transform = undone->transform;
        require(restored == *undone, "transform changed non-pose target data");
        require(target->erase_masks.empty(), "target acquired an unexpected mask");
        const auto initialGeneration = std::stoull(row.at("initial").at("generation").get<std::string>());
        const auto finalGeneration = std::stoull(row.at("final").at("state").at("generation").get<std::string>());
        require(finalGeneration - initialGeneration == (created ? 3U : 2U), "erase/create/transform commit cardinality");
        std::size_t strokes = 0, masksBefore = 0, masksAfter = 0, unchangedMixed = 0;
        std::vector<semantic::Vec2> trace;
        const double initialCaptureEnd = row.at("initialSnapshot").at("readback").at("finished");
        const double mainUp = row.at("mainUp").at("start");
        for (const auto& event : row.at("events")) {
            const double at = event.at("start");
            if (at <= initialCaptureEnd || at >= mainUp) continue;
            const auto phase = event.at("phase").get<std::string>();
            if (phase == "down") trace.clear();
            if (phase == "down" || phase == "move")
                trace.push_back({float(event.at("x").get<double>()), float(event.at("y").get<double>())});
        }
        require(trace.size() == 1601, "confirmed erase waveform cardinality");
        for (const auto& before : initial.objects) {
            const auto* after = find(final, hex(before.id)); require(after, "existing object removed");
            if (before.kind == semantic::ObjectKind::kVectorStroke) {
                ++strokes; auto bodyBefore = before, bodyAfter = *after;
                bodyBefore.erase_masks.clear(); bodyAfter.erase_masks.clear();
                require(bodyBefore == bodyAfter, "long body/transform/material/placement changed");
                require(after->erase_masks.size() == before.erase_masks.size() + 1, "erase mask append cardinality");
                // Snapshot normalization orders masks by identity. Appending an
                // Operation mask need not leave the old masks as a vector prefix.
                for (const auto& mask : before.erase_masks)
                    require(std::find(after->erase_masks.begin(), after->erase_masks.end(), mask) != after->erase_masks.end(),
                            "historical mask identities or geometry changed");
                for (const auto& mask : after->erase_masks) require(!mask.id.isZero(), "mask identity is zero");
                const auto added = std::find_if(after->erase_masks.begin(), after->erase_masks.end(), [&](const auto& mask) {
                    return std::none_of(before.erase_masks.begin(), before.erase_masks.end(), [&](const auto& old) { return old.id == mask.id; });
                });
                require(added != after->erase_masks.end(), "new mask absent");
                const auto* sweep = std::get_if<semantic::SweptCircleMask>(&added->geometry);
                require(sweep && sweep->segments.size() + 1 == trace.size(), "new mask omits confirmed trace");
                require(before.transform.a == 1 && before.transform.b == 0 && before.transform.c == 0 && before.transform.d == 1,
                        "fixture waveform oracle expects translation-only source");
                const auto local = [&](double x, double y) { return semantic::Vec2{x-before.transform.tx, y-before.transform.ty}; };
                const auto near = [](semantic::Vec2 actual, semantic::Vec2 expected) {
                    return std::abs(actual.x-expected.x) <= 1e-6 && std::abs(actual.y-expected.y) <= 1e-6;
                };
                for (std::size_t i = 1; i < trace.size(); ++i) {
                    const auto a = trace[i-1], b = trace[i]; const auto& segment = sweep->segments[i-1];
                    // Input coordinates and the existing confirmed trace use
                    // float, then inverse translation produces canonical doubles.
                    const float ax=a.x, ay=a.y, bx=b.x, by=b.y;
                    require(near(segment.p0.position, local(ax,ay)) && near(segment.p1.position, local(bx,by)) &&
                        near(segment.control1, local(ax+(bx-ax)/3.0F,ay+(by-ay)/3.0F)) &&
                        near(segment.control2, local(ax+(bx-ax)*2.0F/3.0F,ay+(by-ay)*2.0F/3.0F)) &&
                        segment.p0.radius == 16 && segment.p1.radius == 16, "new mask waveform/local coordinates mismatch");
                }
                masksBefore += before.erase_masks.size(); masksAfter += after->erase_masks.size();
            } else if (hex(before.id) == targetId) {
                auto moved = before; moved.transform = after->transform;
                require(moved == *after, "image move changed canonical resource/content");
            } else { require(before == *after, "unrelated mixed object changed"); ++unchangedMixed; }
        }
        require(strokes == 40, "expected 40 dense strokes");
        json material;
        if (kind == "sticky") {
            const auto* content = std::get_if<semantic::StickyContent>(&target->content);
            require(content && content->width == 200 && content->height == 200, "sticky dimensions");
            // The accepted renderer/SDK default for a sticky with no explicit
            // fill property is yellow; an explicit fill overrides that value.
            semantic::ColorValue fillColor{1, 1, .5F, 1};
            for (const auto& property : target->properties.entries) if (property.field_id == 0x100U)
                if (const auto* fill = std::get_if<semantic::FillStyleValue>(&property.value))
                    if (const auto* solid = std::get_if<semantic::SolidFill>(fill)) fillColor = solid->color;
            color(fillColor, row.at("plan").at("color")); material = "canonical/default yellow 200x200";
        } else if (kind == "table") {
            const auto* content = std::get_if<semantic::TableContent>(&target->content);
            require(content && content->rows.size() == 2 && content->columns.size() == 2 && content->cells.size() == 4, "table grid dimensions");
            for (unsigned i = 0; i < 2; ++i) {
                require(content->rows[i].size == 48 && content->columns[i].size == 120, "table axis geometry");
                require(hex(content->rows[i].id) == row.at("typed").at("rows")[i].at("id").get<std::string>(), "table row identity");
                require(hex(content->columns[i].id) == row.at("typed").at("columns")[i].at("id").get<std::string>(), "table column identity");
            }
            color(content->grid.color, row.at("plan").at("color")); material = "canonical 2x2 grid 240x96";
        } else {
            require(kind == "image", "unknown witness kind");
            const auto* content = std::get_if<semantic::ImageContent>(&target->content);
            require(content && content->width == 80 && content->height == 80, "image dimensions");
            require(hex(content->resource_id.value) == row.at("plan").at("resourceId").get<std::string>(), "image resource identity");
            const auto resource = std::find_if(final.manifest.entries.begin(), final.manifest.entries.end(),
                [&](const auto& entry) { return entry.resource_id == content->resource_id; });
            require(resource != final.manifest.entries.end() && resource->content_hash == "sha256:" + row.at("imageActualSHA").get<std::string>(), "image manifest hash");
            material = resource->content_hash;
        }
        std::cout << json{{"status", "QUALIFIED"}, {"kind", kind}, {"target", targetId},
            {"initial_objects", initial.objects.size()}, {"final_objects", final.objects.size()},
            {"unchanged_long_bodies", strokes}, {"historical_masks_preserved", masksBefore}, {"final_masks", masksAfter},
            {"unrelated_mixed_unchanged", unchangedMixed}, {"generation_delta", finalGeneration-initialGeneration},
            {"confirmed_points_per_new_mask", trace.size()}, {"new_mask_waveform_verified", true},
            {"target_material_resource", material}, {"undo_restores_pose_only", true}, {"redo_canonical_equal", true}}
                  << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
