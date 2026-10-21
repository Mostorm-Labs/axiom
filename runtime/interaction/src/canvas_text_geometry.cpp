#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/derived_geometry.hpp"

#include <algorithm>
#include <cmath>
namespace canvas::interaction {
namespace {
bool insideShape(const semantic::ObjectRecord& record, foundation::WorldPoint point) {
    const auto* shape = std::get_if<semantic::ShapeContent>(&record.content);
    if (!shape || record.kind != semantic::ObjectKind::kShape) return false;
    const auto& t = record.transform;
    const auto det = t.a * t.d - t.b * t.c;
    if (!std::isfinite(det) || std::abs(det) < 1e-12) return false;
    const double dx = point.x - t.tx, dy = point.y - t.ty;
    const double x = (t.d * dx - t.c * dy) / det;
    const double y = (t.a * dy - t.b * dx) / det;
    if (x < 0 || y < 0 || x > shape->width || y > shape->height) return false;
    if (shape->shape_kind == 2U) {
        const auto nx = 2 * x / shape->width - 1, ny = 2 * y / shape->height - 1;
        return nx * nx + ny * ny <= 1;
    }
    if (shape->shape_kind == 3U) {
        const auto triangle = scene::triangleGeometry(*shape);
        return triangle && scene::pointInTriangle(
            {static_cast<float>(x), static_cast<float>(y)}, *triangle);
    }
    return shape->shape_kind == 1U;
}
} // namespace
void CanvasEditingRuntime::updateTextCaretPresentation(
    std::chrono::steady_clock::time_point now) noexcept {
    const auto state = interaction_->textSession()
                           ? interaction_->textSession()->read()
                           : runtime::TextEditorSnapshot{};
    const bool active = state.phase != runtime::TextPhase::kClosed;
    const bool changed = state.token != caretToken_ || state.selection != caretSelection_ ||
                         state.preeditSelection != caretPreeditSelection_ ||
                         state.phase != caretPhase_ ||
                         state.nextInputSequence != caretInputSequence_;
    const bool wasActive = caretPhase_ != runtime::TextPhase::kClosed;
    if (changed) {
        caretToken_ = state.token;
        caretSelection_ = state.selection;
        caretPreeditSelection_ = state.preeditSelection;
        caretPhase_ = state.phase;
        caretInputSequence_ = state.nextInputSequence;
        caretBlinkStart_ = now;
    }
    // Use elapsed monotonic time, not host frame counts. Input restores a
    // visible caret immediately; idle owner pumps only redraw at a phase edge.
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - caretBlinkStart_).count();
    const bool visible = active && (elapsed / 500) % 2 == 0;
    if (visible != textCaretVisible_ || (changed && (active || wasActive)))
        canonicalPresentationDirty_ = true;
    textCaretVisible_ = visible;
}
std::string CanvasEditingRuntime::textFontName(semantic::ObjectId id) const {
    return textLayoutService_ ? textLayoutService_->fontFamilyName(id) : std::string{};
}
std::vector<std::uint32_t> CanvasEditingRuntime::textGraphemes(const std::u16string& text) const {
    return textLayoutService_ ? textLayoutService_->graphemeBoundaries(text)
                              : std::vector<std::uint32_t>{};
}
std::optional<std::uint32_t>
CanvasEditingRuntime::textHit(const semantic::ObjectRecord& record, float x, float y) {
    if (!textLayoutService_)
        return {};
    const auto point = viewToContent(x, y);
    const auto& t = record.transform;
    const auto det = t.a * t.d - t.b * t.c;
    if (!std::isfinite(det) || std::abs(det) < 1e-12)
        return {};
    const double dx = point.first - t.tx, dy = point.second - t.ty;
    const auto* content = std::get_if<semantic::RichTextContent>(&record.content);
    if (const auto* shape = std::get_if<semantic::ShapeContent>(&record.content);
        shape != nullptr && shape->label.has_value()) content = &*shape->label;
    const auto* shape = std::get_if<semantic::ShapeContent>(&record.content);
    if (content == nullptr && !shape) return {};
    foundation::WorldPoint local{static_cast<float>((t.d * dx - t.c * dy) / det),
                                 static_cast<float>((t.a * dy - t.b * dx) / det)};
    text::EditingGeometry geometry;
    if (shape) {
        const float sx = std::max(0.001F, std::hypot(static_cast<float>(t.a), static_cast<float>(t.b)));
        const float sy = std::max(0.001F, std::hypot(static_cast<float>(t.c), static_cast<float>(t.d)));
        local.x *= sx; local.y *= sy;
        geometry = textLayoutService_->shapeEditingGeometry(record, 0, 0, local);
    } else {
        geometry = textLayoutService_->editingGeometry(record.id, *content, 0, 0, local);
    }
    return geometry.available ? geometry.hit : std::nullopt;
}
semantic::ObjectId CanvasEditingRuntime::hitTextAtView(float x, float y) {
    if (!textLayoutService_)
        return {};
    auto objects = semanticObjects_.allObjects();
    std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) {
        return a.placement.order_key > b.placement.order_key;
    });
    const auto point = viewToContent(x, y);
    for (const auto& o : objects) {
        if (!textWritable(o.id)) continue;
        const auto* content = std::get_if<semantic::RichTextContent>(&o.content);
        if (const auto* shape = std::get_if<semantic::ShapeContent>(&o.content);
            shape != nullptr) {
            if (insideShape(o, {point.first, point.second})) return o.id;
            continue;
        }
        if (content == nullptr) continue;
        const auto layout = textLayoutService_->resolve(o.id, *content);
        if (!layout->diagnostics.empty())
            continue;
        const auto bounds = canvas::scene::computeBounds(o, layout.get()).world;
        if (point.first >= bounds.left && point.first <= bounds.right &&
            point.second >= bounds.top && point.second <= bounds.bottom)
            return o.id;
    }
    return {};
}
bool CanvasEditingRuntime::shapeExpandPointer(const input::PointerSample& sample) {
    if (sample.phase != input::PointerPhase::kDown) {
        if (!shapeExpandPointerKeys_.contains(sample.key)) return false;
        if (sample.phase == input::PointerPhase::kUp ||
            sample.phase == input::PointerPhase::kCancel)
            shapeExpandPointerKeys_.erase(sample.key);
        return true;
    }
    if (!sample.key.valid() || !textLayoutService_) return false;
    const bool active = interaction_->textSession() &&
        interaction_->textSession()->read().phase != runtime::TextPhase::kClosed;
    if (!textTool_ && !selectionMode_ && !active) return false;
    const auto point = viewToContent(sample.x, sample.y);
    for (const auto& candidate : semanticObjects_.allObjects()) {
        const auto* shape = std::get_if<semantic::ShapeContent>(&candidate.content);
        if (candidate.kind != semantic::ObjectKind::kShape || !shape || !shape->label ||
            !textWritable(candidate.id)) continue;
        const auto& transform = candidate.transform;
        const double det = transform.a * transform.d - transform.b * transform.c;
        if (!std::isfinite(det) || std::abs(det) < 1.0e-12) continue;
        const float sx = std::max(0.001F, std::hypot(static_cast<float>(transform.a),
                                                      static_cast<float>(transform.b)));
        const float sy = std::max(0.001F, std::hypot(static_cast<float>(transform.c),
                                                      static_cast<float>(transform.d)));
        const double dx = point.first - transform.tx, dy = point.second - transform.ty;
        const float px = static_cast<float>((transform.d * dx - transform.c * dy) / det) * sx;
        const float py = static_cast<float>((transform.a * dy - transform.b * dx) / det) * sy;
        const auto layout = textLayoutService_->resolveShapeLabel(
            candidate.id, *shape->label, *shape, transform);
        if (!layout || !layout->overflow || px < layout->expandButton.left ||
            px > layout->expandButton.right || py < layout->expandButton.top ||
            py > layout->expandButton.bottom) continue;
        shapeExpandPointerKeys_.insert(sample.key);
        (void)expandShapeLabel(candidate.id);
        return true;
    }
    return false;
}
std::optional<semantic::ObjectRecord> CanvasEditingRuntime::textPresentation() {
    if (!interaction_->textSession()) return {};
    const auto state = interaction_->textSession()->read();
    auto record = interaction_->textSession()->presentation();
    if (!record) return {};
    if (const auto* current = semanticObjects_.find(record->id))
        record->transform = current->transform;
    // Native preedit is only a presentation. It can expand the drawn Shape,
    // but cancellation must leave canonical geometry and history untouched.
    if (state.composition)
        if (const auto fitted = fittedShapeLabelTransform(*record))
            record->transform = *fitted;
    return record;
}
runtime::TextGeometrySnapshot
CanvasEditingRuntime::textGeometry(const runtime::CanvasTargetKey& target) {
    runtime::TextGeometrySnapshot out;
    if (!interaction_->textSession() || !textLayoutService_ || !surface_.available)
        return out;
    auto& editor = *interaction_->textSession();
    const auto state = editor.read();
    const auto record = textPresentation();
    if (!record)
        return out;
    const auto start =
        state.composition
            ? state.replacement.start + (state.preeditSelection
                                             ? state.preeditSelection->start
                                             : static_cast<std::uint32_t>(state.preedit.size()))
            : state.selection.start;
    const auto length = state.composition ? 0 : state.selection.length;
    const auto* content = std::get_if<semantic::RichTextContent>(&record->content);
    if (const auto* shape = std::get_if<semantic::ShapeContent>(&record->content);
        shape != nullptr && shape->label.has_value()) content = &*shape->label;
    const auto* shape = std::get_if<semantic::ShapeContent>(&record->content);
    if (content == nullptr && !shape) return out;
    const auto geometry = shape
        ? textLayoutService_->shapeEditingGeometry(*record, start, length)
        : textLayoutService_->editingGeometry(record->id, *content, start, length);
    out.available = geometry.available;
    out.diagnostics = geometry.diagnostics;
    out.token = {state.token,
                 target,
                 cameraGeneration(),
                 geometry.layoutIdentity + ":" + std::to_string(start) + ":" +
                     std::to_string(length)};
    const auto view = viewportController_->state();
    const auto t = record->transform;
    const float labelScaleX = shape ? std::max(0.001F, std::hypot(static_cast<float>(t.a), static_cast<float>(t.b))) : 1.0F;
    const float labelScaleY = shape ? std::max(0.001F, std::hypot(static_cast<float>(t.c), static_cast<float>(t.d))) : 1.0F;
    const auto quad = [&](foundation::WorldRect r) {
        std::array<float, 8> q{};
        const std::array<foundation::WorldPoint, 4> points{
            {{r.left, r.top}, {r.right, r.top}, {r.right, r.bottom}, {r.left, r.bottom}}};
        for (std::size_t i = 0; i < 4; ++i) {
            const float x = points[i].x / labelScaleX, y = points[i].y / labelScaleY;
            q[i * 2] = static_cast<float>(
                (t.a * x + t.c * y + t.tx) * view.scale + view.translationX);
            q[i * 2 + 1] = static_cast<float>(
                (t.b * x + t.d * y + t.ty) * view.scale + view.translationY);
        }
        return q;
    };
    if (out.available) {
        out.caret = quad(geometry.caret);
        for (const auto& r : geometry.selection)
            out.selection.push_back(quad(r));
    }
    return out;
}
bool CanvasEditingRuntime::textPointer(const input::PointerSample& sample) {
    auto* editor = interaction_->textSession();
    if (!editor)
        return false;
    auto state = editor->read();
    const bool active = state.phase != runtime::TextPhase::kClosed;
    if (sample.phase == input::PointerPhase::kDown && (textTool_ || active)) {
        const float x = sample.x, y = sample.y;
        const auto existing = hitTextAtView(x, y);
        if (!active && !existing.isZero()) {
            const auto* record = semanticObjects_.find(existing);
            const auto* shape = record ? std::get_if<semantic::ShapeContent>(&record->content) : nullptr;
            if (shape && shape->label) {
                textPointerKeys_.insert(sample.key);
                textSelectionPointer_ = sample.key;
                return true;
            }
        }
        textPointerKeys_.insert(sample.key);
        if (textPointerKeys_.size() > 1)
            return true;
        textSelectionPointer_ = sample.key;
        if (!active) {
            runtime::TextIntent create;
            const auto existingForEdit = hitTextAtView(x, y);
            create.kind = existingForEdit.isZero() ? runtime::TextIntentKind::kCreate
                                                    : runtime::TextIntentKind::kEdit;
            create.object = existingForEdit;
            create.viewX = sample.x;
            create.viewY = sample.y;
            (void)editor->intent(create);
        } else if (state.composition)
            (void)editor->finish();
        else if (const auto record = textPresentation()) {
            if (const auto hit = textHit(*record, sample.x, sample.y)) {
                runtime::TextInputEvent selection;
                selection.token = state.token;
                selection.sequence = state.nextInputSequence;
                selection.range = {*hit, 0};
                textSelectionAnchor_ = *hit;
                selection.kind = runtime::TextInputKind::kSelection;
                (void)editor->input(selection);
            }
        }
        canonicalPresentationDirty_ = true;
        return true;
    }
    if (sample.phase == input::PointerPhase::kMove && sample.key == textSelectionPointer_ &&
        !state.composition) {
        if (const auto record = textPresentation())
            if (const auto hit = textHit(*record, sample.x, sample.y)) {
                runtime::TextInputEvent selection;
                selection.token = state.token;
                selection.sequence = state.nextInputSequence;
                selection.kind = runtime::TextInputKind::kSelection;
                selection.range = {
                    std::min(textSelectionAnchor_, *hit),
                    static_cast<std::uint32_t>(std::max(textSelectionAnchor_, *hit) -
                                               std::min(textSelectionAnchor_, *hit))};
                (void)editor->input(selection);
                canonicalPresentationDirty_ = true;
            }
    }
    const bool owned = textPointerKeys_.contains(sample.key);
    if (sample.phase == input::PointerPhase::kUp || sample.phase == input::PointerPhase::kCancel)
        textPointerKeys_.erase(sample.key);
    return owned || active;
}
} // namespace canvas::interaction
