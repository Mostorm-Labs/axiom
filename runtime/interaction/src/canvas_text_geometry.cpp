#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/scene/bounds_system.hpp"

#include <algorithm>
#include <cmath>
namespace canvas::interaction {
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
    const auto geometry = textLayoutService_->editingGeometry(
        record.id,
        std::get<semantic::RichTextContent>(record.content),
        0,
        0,
        foundation::WorldPoint{static_cast<float>((t.d * dx - t.c * dy) / det),
                               static_cast<float>((t.a * dy - t.b * dx) / det)});
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
        if (o.kind != semantic::ObjectKind::kRichText || !textWritable(o.id))
            continue;
        const auto layout =
            textLayoutService_->resolve(o.id, std::get<semantic::RichTextContent>(o.content));
        if (!layout->diagnostics.empty())
            continue;
        const auto bounds = canvas::scene::computeBounds(o, layout.get()).world;
        if (point.first >= bounds.left && point.first <= bounds.right &&
            point.second >= bounds.top && point.second <= bounds.bottom)
            return o.id;
    }
    return {};
}
runtime::TextGeometrySnapshot
CanvasEditingRuntime::textGeometry(const runtime::CanvasTargetKey& target) {
    runtime::TextGeometrySnapshot out;
    if (!interaction_->textSession() || !textLayoutService_ || !surface_.available)
        return out;
    auto& editor = *interaction_->textSession();
    const auto state = editor.read();
    const auto record = editor.presentation();
    if (!record)
        return out;
    const auto start =
        state.composition
            ? state.replacement.start + (state.preeditSelection
                                             ? state.preeditSelection->start
                                             : static_cast<std::uint32_t>(state.preedit.size()))
            : state.selection.start;
    const auto length = state.composition ? 0 : state.selection.length;
    const auto geometry = textLayoutService_->editingGeometry(
        record->id, std::get<semantic::RichTextContent>(record->content), start, length);
    out.available = geometry.available;
    out.diagnostics = geometry.diagnostics;
    out.token = {state.token,
                 target,
                 cameraGeneration(),
                 geometry.layoutIdentity + ":" + std::to_string(start) + ":" +
                     std::to_string(length)};
    const auto view = viewportController_->state();
    const auto t = record->transform;
    const auto quad = [&](foundation::WorldRect r) {
        std::array<float, 8> q{};
        const std::array<foundation::WorldPoint, 4> points{
            {{r.left, r.top}, {r.right, r.top}, {r.right, r.bottom}, {r.left, r.bottom}}};
        for (std::size_t i = 0; i < 4; ++i) {
            q[i * 2] = static_cast<float>(
                (t.a * points[i].x + t.c * points[i].y + t.tx) * view.scale + view.translationX);
            q[i * 2 + 1] = static_cast<float>(
                (t.b * points[i].x + t.d * points[i].y + t.ty) * view.scale + view.translationY);
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
        textPointerKeys_.insert(sample.key);
        if (textPointerKeys_.size() > 1)
            return true;
        textSelectionPointer_ = sample.key;
        if (!active) {
            runtime::TextIntent create;
            create.kind = runtime::TextIntentKind::kCreate;
            create.viewX = sample.x;
            create.viewY = sample.y;
            (void)editor->intent(create);
        } else if (state.composition)
            (void)editor->finish();
        else if (const auto record = editor->presentation()) {
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
        if (const auto record = editor->presentation())
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
