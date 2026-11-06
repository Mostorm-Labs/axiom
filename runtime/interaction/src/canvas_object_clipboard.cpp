#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include "canvas/semantic/codec.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace canvas::interaction {
namespace {
using namespace semantic;
using namespace runtime;
constexpr std::size_t maxBytes = 16U * 1024U * 1024U, maxObjects = 4096;
void put32(std::vector<std::uint8_t>& out, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(n >> (i * 8)));
}
std::uint32_t get32(const std::vector<std::uint8_t>& in, std::size_t at) {
    std::uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i) n |= std::uint32_t(in.at(at + i)) << (i * 8);
    return n;
}
Operation carrier(DocumentId document, OperationPayload payload) {
    // A serialization carrier only. Never admitted/replayed as this Operation.
    return {OperationId{foundation::ObjectId::fromUint64(1)}, document, 1, 1, std::move(payload)};
}
void textResources(const RichTextContent& rich, std::set<foundation::ObjectId>& ids) {
    for (const auto& p : rich.document.paragraphs) for (const auto& r : p.runs)
        if (r.style.font_resource_id) ids.insert(r.style.font_resource_id->value);
}
std::set<foundation::ObjectId> resourceIds(const std::vector<ObjectRecord>& records) {
    std::set<foundation::ObjectId> ids;
    for (const auto& o : records) std::visit([&](const auto& c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, ImageContent>) ids.insert(c.resource_id.value);
        else if constexpr (std::is_same_v<T, RichTextContent>) textResources(c, ids);
        else if constexpr (std::is_same_v<T, ShapeContent>) { if (c.label) textResources(*c.label, ids); }
        else if constexpr (std::is_same_v<T, TableContent>) { for (const auto& cell : c.cells) textResources(cell.text, ids); }
        else if constexpr (std::is_same_v<T, VectorStrokeContent> || std::is_same_v<T, DabStrokeContent>) {
            if (c.stroke.brush.texture_resource_id) ids.insert(c.stroke.brush.texture_resource_id->value);
        } else if constexpr (std::is_same_v<T, BrushStrokeContent> || std::is_same_v<T, DabBrushStrokeContent>) {
            for (const auto& r : c.stroke.snapshot.resources) ids.insert(r.resource_id);
        }
    }, o.content);
    return ids;
}
const ResourceManifestEntry* resource(const ResourceManifest& manifest, foundation::ObjectId id) {
    const auto it = std::find_if(manifest.entries.begin(), manifest.entries.end(), [&](const auto& e) { return e.resource_id.value == id; });
    return it == manifest.entries.end() ? nullptr : &*it;
}
bool nextOrder(std::vector<std::uint8_t>& key) {
    if (key.empty()) { key.push_back(128); return true; }
    for (std::size_t n = key.size(); n > 0; --n) if (key[n - 1] != 255) {
        ++key[n - 1]; key.resize(n); return true;
    }
    if (key.size() == OrderKey::kMaxSize) return false;
    key.push_back(128); return true;
}
}

runtime::ClipboardResult CanvasEditingRuntime::clipboardCommand(
    const runtime::ClipboardRequest& request, const runtime::MultiSelectionProjection& current) {
    ClipboardResult out; out.generation = semanticGeneration_.current().value(); out.stamp = current.stamp;
    const auto fail = [&](ClipboardError error) { out.error = error; return out; };
    if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_ || !current.available ||
        !multiSelectionPermissionAllowed_ || !textReady() || appearanceSession_ || imageCropPresentation_ ||
        regionSelectionActive() || (interaction_->textSession() && interaction_->textSession()->read().phase != TextPhase::kClosed))
        return fail(ClipboardError::kBusy);
    if ((request.action == ClipboardAction::kCut && !request.stamp) || (request.stamp && *request.stamp != current.stamp)) return fail(ClipboardError::kStale);
    if (!std::isfinite(request.offsetX) || !std::isfinite(request.offsetY) ||
        std::abs(request.offsetX) > 1e9 || std::abs(request.offsetY) > 1e9) return fail(ClipboardError::kInvalid);
    if (static_cast<unsigned>(request.action) > static_cast<unsigned>(ClipboardAction::kPaste)) return fail(ClipboardError::kInvalid);
    std::vector<ObjectRecord> records;
    std::vector<ResourceManifestEntry> dependencies;
    std::vector<foundation::ObjectId> sourceRoots;
    if (request.action != ClipboardAction::kPaste) {
        const auto roots = selection_.resolveRoots(selection_.selected(), semanticObjects_);
        if (!roots || roots->empty()) return fail(ClipboardError::kInvalid);
        sourceRoots = *roots;
        std::sort(sourceRoots.begin(),sourceRoots.end(),[&](auto a,auto b){
            const auto* left=sceneCoordinator_->runtimeScene().find(a);
            const auto* right=sceneCoordinator_->runtimeScene().find(b);
            if(left&&right&&left->hierarchyOrder&&right->hierarchyOrder&&left->hierarchyOrder!=right->hierarchyOrder)return left->hierarchyOrder<right->hierarchyOrder;
            const auto& x=semanticObjects_.find(a)->placement.order_key;
            const auto& y=semanticObjects_.find(b)->placement.order_key;
            return x==y?a<b:x<y;
        });
        std::set<foundation::ObjectId> closure(roots->begin(), roots->end());
        std::vector<foundation::ObjectId> pending = sourceRoots;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            if (pending.size() > maxObjects) return fail(ClipboardError::kUnsupported);
            const auto* original = semanticObjects_.find(pending[i]);
            if (!original) return fail(ClipboardError::kStale);
            if (!textWritable(original->id)) return fail(ClipboardError::kLocked);
            records.push_back(*original);
            for (const auto& child : semanticObjects_.children(original->id))
                if (closure.insert(child.id).second) pending.push_back(child.id);
        }
        for (auto& o : records) {
            const auto* world = sceneCoordinator_->runtimeScene().find(o.id);
            if (!world) return fail(ClipboardError::kBusy);
            if (std::find(roots->begin(), roots->end(), o.id) != roots->end()) {
                o.placement.parent_id.reset(); o.transform = world->transform;
                // Canonical serialization sorts records by ID. Encode the
                // captured cross-root paint order in detached Placement keys.
                const auto rank=static_cast<std::size_t>(std::find(sourceRoots.begin(),sourceRoots.end(),o.id)-sourceRoots.begin());
                o.placement.order_key=OrderKey({static_cast<std::uint8_t>(rank>>8),static_cast<std::uint8_t>(rank),128});
            }
            if (auto* c = std::get_if<ConnectorContent>(&o.content)) {
                const auto* route = std::get_if<scene::ConnectorGeometry>(&world->derivedGeometry);
                for (auto* e : {&c->start, &c->end}) if (const auto* a = std::get_if<AttachedEndpoint>(&e->value); a && !closure.contains(a->target_object_id)) {
                    Transform2D inverse;
                    if (!route || !scene::inverseTransform(world->transform, inverse)) return fail(ClipboardError::kUnsupported);
                    const auto p = e == &c->start ? route->start : route->end;
                    e->value = FreePointEndpoint{{inverse.a * p.x + inverse.c * p.y + inverse.tx, inverse.b * p.x + inverse.d * p.y + inverse.ty}};
                }
            }
        }
        auto ids = resourceIds(records);
        for (auto it = ids.begin(); it != ids.end(); ++it) {
            const auto* entry = resource(*resourceManifestPort_, *it);
            if (!entry) return fail(ClipboardError::kResourceUnavailable);
            dependencies.push_back(*entry);
            if (const auto fallback = fontFallbackResource(entry->metadata)) ids.insert(fallback->value);
        }
        // A fallback may sort before its referring font in the set.
        for (const auto id : ids) if (std::none_of(dependencies.begin(), dependencies.end(), [&](const auto& e) { return e.resource_id.value == id; })) {
            const auto* entry = resource(*resourceManifestPort_, id);
            if (!entry) return fail(ClipboardError::kResourceUnavailable);
            dependencies.push_back(*entry);
        }
        out.objectCount = static_cast<std::uint32_t>(records.size()); out.roots = sourceRoots;
        if(request.action==ClipboardAction::kDuplicate) {
            const auto measured=SemanticCodec::measureProtobufOperation(carrier(documentId_,InsertObjectsOp{records}));
            if(!measured.ok()||measured.bytes+16>maxBytes)return fail(ClipboardError::kUnsupported);
        }
        if (request.action == ClipboardAction::kCopy) {
            auto encoded = SemanticCodec::encodeProtobufOperation(carrier(documentId_, InsertObjectsOp{records}));
            CodecResult manifest;
            if (!dependencies.empty()) { UpdateResourceManifestOp m; m.upserts = dependencies; manifest = SemanticCodec::encodeProtobufOperation(carrier(documentId_, std::move(m))); }
            if (!encoded.ok() || !manifest.ok()) return fail(ClipboardError::kUnsupported);
            if (encoded.bytes.size() + manifest.bytes.size() + 16 > maxBytes) return fail(ClipboardError::kUnsupported);
            out.bytes = {'A','X','C','B'}; put32(out.bytes, 1); put32(out.bytes, static_cast<std::uint32_t>(encoded.bytes.size())); put32(out.bytes, static_cast<std::uint32_t>(manifest.bytes.size()));
            out.bytes.insert(out.bytes.end(), encoded.bytes.begin(), encoded.bytes.end()); out.bytes.insert(out.bytes.end(), manifest.bytes.begin(), manifest.bytes.end());
            out.applied = true; out.error = ClipboardError::kNone; return out;
        }
    } else {
        const auto& b = request.bytes;
        if (b.size() < 16 || b.size() > maxBytes || !std::equal(b.begin(), b.begin() + 4, "AXCB")) return fail(ClipboardError::kInvalid);
        if (get32(b, 4) != 1) return fail(ClipboardError::kUnsupported);
        const auto length = get32(b, 8), manifestLength = get32(b, 12);
        if (std::uint64_t(length) + manifestLength + 16 != b.size()) return fail(ClipboardError::kInvalid);
        auto data = SemanticCodec::decodeProtobufOperation({b.begin() + 16, b.begin() + 16 + length});
        if (!data.ok()) return fail(ClipboardError::kInvalid);
        const auto* insert = std::get_if<InsertObjectsOp>(&data.operation.payload);
        if (!insert || insert->objects.empty() || insert->objects.size() > maxObjects) return fail(ClipboardError::kInvalid);
        records = insert->objects;
        if (manifestLength) {
            auto decoded = SemanticCodec::decodeProtobufOperation({b.begin() + 16 + length, b.end()});
            if (!decoded.ok()) return fail(ClipboardError::kInvalid);
            const auto* m = std::get_if<UpdateResourceManifestOp>(&decoded.operation.payload);
            if (!m || !m->removes.empty() || !m->reactivations.empty()) return fail(ClipboardError::kInvalid);
            dependencies = m->upserts;
        }
        const auto needed = resourceIds(records);
        for (const auto id : needed) if (std::none_of(dependencies.begin(), dependencies.end(), [&](const auto& e) { return e.resource_id.value == id; })) return fail(ClipboardError::kInvalid);
        for (const auto& entry : dependencies) {
            const auto* existing = resource(*resourceManifestPort_, entry.resource_id.value);
            if (!existing || *existing != entry) return fail(ClipboardError::kResourceUnavailable);
            if (const auto fallback = fontFallbackResource(entry.metadata); fallback && std::none_of(dependencies.begin(), dependencies.end(), [&](const auto& e) { return e.resource_id == *fallback; })) return fail(ClipboardError::kInvalid);
        }
    }
    if (request.action == ClipboardAction::kCut) {
        const auto forward = allocateOperationId(), inverseId = allocateOperationId();
        Operation operation{forward, documentId_, 1, 1, DeleteObjectsOp{sourceRoots}};
        const auto plan = operationEngine_.prepare(operation, {semanticObjects_, appliedOperations_, resourceManifestPort_});
        if (plan.disposition != PrepareDisposition::kPrepared || !plan.plan) return fail(ClipboardError::kInvalid);
        RestoreObjectsOp restore;
        for (auto id : plan.plan->deletes) {
            const auto* o = semanticObjects_.find(id);
            if (!o || !textWritable(id)) return fail(ClipboardError::kLocked);
            restore.objects.push_back(*o);
        }
        if (!applyLocalHistoryOperation(operation, {inverseId, documentId_, 1, 1, std::move(restore)})) return fail(ClipboardError::kBusy);
        selection_.cancel(); selectionView_.reset();
    } else {
        std::stable_sort(records.begin(),records.end(),[](const auto& a,const auto& b){
            if(bool(a.placement.parent_id)!=bool(b.placement.parent_id))return !a.placement.parent_id;
            if(!a.placement.parent_id&&a.placement.order_key!=b.placement.order_key)return a.placement.order_key<b.placement.order_key;
            return a.id<b.id;
        });
        std::map<foundation::ObjectId, foundation::ObjectId> remap;
        for (const auto& o : records) if (!remap.emplace(o.id, allocateTextIdentity()).second) return fail(ClipboardError::kInvalid);
        std::map<std::optional<foundation::ObjectId>, std::vector<std::uint8_t>> orders;
        std::vector<foundation::ObjectId> createdRoots;
        for (auto& o : records) {
            const auto original = o.id; o.id = remap.at(original);
            if (o.id.isZero()) return fail(ClipboardError::kUnsupported);
            if (o.placement.parent_id) {
                const auto found = remap.find(*o.placement.parent_id);
                if (found == remap.end()) return fail(ClipboardError::kInvalid);
                o.placement.parent_id = found->second;
            } else {
                auto parent = selection_.groupScope();
                if (request.action == ClipboardAction::kDuplicate) parent = semanticObjects_.find(original)->placement.parent_id;
                if (parent && !textWritable(*parent)) return fail(ClipboardError::kLocked);
                o.transform.tx += request.offsetX; o.transform.ty += request.offsetY;
                if (parent) {
                    const auto* world = sceneCoordinator_->runtimeScene().find(*parent); Transform2D inverse;
                    if (!world || !scene::inverseTransform(world->transform, inverse)) return fail(ClipboardError::kInvalid);
                    o.transform = scene::composeTransform(inverse, o.transform);
                }
                o.placement.parent_id = parent;
                auto [it, first] = orders.try_emplace(parent);
                if (first) { const auto siblings = semanticObjects_.children(parent); if (!siblings.empty()) it->second.assign(siblings.back().placement.order_key.bytes().begin(), siblings.back().placement.order_key.bytes().end()); }
                if (!nextOrder(it->second)) return fail(ClipboardError::kUnsupported);
                o.placement.order_key = OrderKey(it->second); createdRoots.push_back(o.id);
            }
            for (auto& mask : o.erase_masks) mask.id = allocateTextIdentity();
            const auto rich = [&](RichTextContent& r) { for (auto& p : r.document.paragraphs) p.id = allocateTextIdentity(); };
            if (auto* r = std::get_if<RichTextContent>(&o.content)) rich(*r);
            if (auto* s = std::get_if<ShapeContent>(&o.content); s && s->label) rich(*s->label);
            if (auto* t = std::get_if<TableContent>(&o.content)) { for (auto& a : t->rows) a.id = allocateTextIdentity(); for (auto& a : t->columns) a.id = allocateTextIdentity(); for (auto& c : t->cells) rich(c.text); }
            if (auto* c = std::get_if<ConnectorContent>(&o.content)) for (auto* e : {&c->start, &c->end}) if (auto* a = std::get_if<AttachedEndpoint>(&e->value)) {
                const auto found = remap.find(a->target_object_id);
                if (found == remap.end()) return fail(ClipboardError::kInvalid);
                a->target_object_id = found->second;
            }
        }
        if (createdRoots.empty()) return fail(ClipboardError::kInvalid);
        std::vector<foundation::ObjectId> ids; for (const auto& o : records) ids.push_back(o.id);
        // Prepare UI selection before canonical publication; failure is zero-mutation.
        auto afterSelection = selection_;
        if (!afterSelection.applyAcquisition(createdRoots, SelectionCombineMode::kReplace, afterSelection.revision())) return fail(ClipboardError::kInvalid);
        const auto forward = allocateOperationId(), inverse = allocateOperationId();
        if (!applyLocalHistoryOperation({forward, documentId_, 1, 1, InsertObjectsOp{records}}, {inverse, documentId_, 1, 1, DeleteObjectsOp{ids}})) return fail(ClipboardError::kBusy);
        (void)setSelectionAcquisitionMode(CanvasSelectionMode::kPoint); selection_ = std::move(afterSelection);
        out.roots = std::move(createdRoots); out.objectCount = static_cast<std::uint32_t>(records.size());
    }
    (void)clearCreationHover(); canonicalPresentationDirty_ = true;
    out.changed = true; out.applied = true; out.error = ClipboardError::kNone;
    out.generation = semanticGeneration_.current().value(); return out;
}
}
