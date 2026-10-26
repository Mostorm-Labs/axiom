#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"

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
    if(!historySceneReady())return {};
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
        if (record.placement.parent_id) {
            const auto* parent = semanticObjects_.find(*record.placement.parent_id);
            if (parent != nullptr && parent->kind == semantic::ObjectKind::kSticky) {
                if (const auto* sticky = std::get_if<semantic::StickyContent>(&parent->content))
                    geometry = textLayoutService_->stickyEditingGeometry(
                        record.id, *content, *sticky, 0, 0, local,
                        parent == nullptr ? semantic::Transform2D{} : sceneCoordinator_->runtimeScene().find(parent->id)->transform);
                else
                    geometry = textLayoutService_->editingGeometry(record.id, *content, 0, 0, local);
            } else {
                geometry = textLayoutService_->editingGeometry(record.id, *content, 0, 0, local);
            }
        } else {
            geometry = textLayoutService_->editingGeometry(record.id, *content, 0, 0, local);
        }
    }
    return geometry.available ? geometry.hit : std::nullopt;
}
semantic::ObjectId CanvasEditingRuntime::hitTextAtView(float x, float y) {
    if(!historySceneReady())return {};
    if (!textLayoutService_)
        return {};
    auto objects = scene::projectWorldObjects(semanticObjects_.allObjects());
    std::reverse(objects.begin(),objects.end());
    const auto point = viewToContent(x, y);
    for (const auto& o : objects) {
        if (!textWritable(o.id)) continue;
        if (o.kind == semantic::ObjectKind::kSticky) {
            const auto* sticky = std::get_if<semantic::StickyContent>(&o.content);
            if (!sticky) continue;
            const auto& t = o.transform;
            const auto det = t.a * t.d - t.b * t.c;
            if (!std::isfinite(det) || std::abs(det) < 1e-12) continue;
            const double dx = point.first - t.tx, dy = point.second - t.ty;
            const float localX = static_cast<float>((t.d * dx - t.c * dy) / det);
            const float localY = static_cast<float>((t.a * dy - t.b * dx) / det);
            if (localX >= 0.0F && localY >= 0.0F && localX <= sticky->width && localY <= sticky->height)
                return o.id;
            continue;
        }
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
    // The expand affordance is a selection affordance. Text tool/editing may
    // also own it, but a selected Sticky/Shape must be enough to activate it.
    if (!textTool_ && !selectionMode_ && !active && selection_.primary().isZero()) return false;
    const auto point = viewToContent(sample.x, sample.y);
    // Selection chrome normally owns the parent Sticky, but Text tool input
    // can leave the active RichText child as the only current identity.  Use
    // the editor token as a fallback so the affordance remains clickable while
    // editing, exactly like the Shape label button.
    auto selectedId = selection_.primary();
    if (selectedId.isZero() && active)
        selectedId = interaction_->textSession()->read().token.object;
    const auto* selectedObject = selectedId.isZero() ? nullptr : semanticObjects_.find(selectedId);
    auto selectedSticky = selectedObject != nullptr &&
        selectedObject->kind == semantic::ObjectKind::kSticky
        ? selectedObject->id : semantic::ObjectId{};
    if (selectedSticky.isZero() && selectedObject != nullptr &&
        selectedObject->kind == semantic::ObjectKind::kRichText &&
        selectedObject->placement.parent_id) {
        const auto* parent = semanticObjects_.find(*selectedObject->placement.parent_id);
        if (parent != nullptr && parent->kind == semantic::ObjectKind::kSticky)
            selectedSticky = parent->id;
    }
    for (const auto& candidate : scene::projectWorldObjects(semanticObjects_.allObjects())) {
        if (candidate.kind == semantic::ObjectKind::kRichText && candidate.placement.parent_id &&
            selectedSticky == *candidate.placement.parent_id && textLayoutService_) {
            const auto* parent = semanticObjects_.find(*candidate.placement.parent_id);
            const auto* sticky = parent ? std::get_if<semantic::StickyContent>(&parent->content) : nullptr;
            const auto* rich = std::get_if<semantic::RichTextContent>(&candidate.content);
            if (parent && sticky && rich) {
                const auto* overrideTransform = transformOverrides_.find(parent->id);
                const auto visualTransform = overrideTransform != nullptr
                    ? *overrideTransform : sceneCoordinator_->runtimeScene().find(parent->id)->transform;
                const auto layout = textLayoutService_->resolveStickyLabel(candidate.id, *rich, *sticky, true,
                                                                            visualTransform);
                if (layout && layout->overflow) {
                    const auto& transform = visualTransform;
                    const double det = transform.a * transform.d - transform.b * transform.c;
                    if (std::isfinite(det) && std::abs(det) >= 1.0e-12) {
                        const double dx = point.first - transform.tx, dy = point.second - transform.ty;
                        // Sticky layout snapshots use world-size coordinates
                        // so text can be inverse-scaled like Shape labels.
                        // The inverse transform returns Sticky-local
                        // coordinates.  resolveStickyLabel stores its
                        // affordance in visible/world-size label space, so
                        // convert once by the current basis before testing.
                        const float sx = std::max(0.001F,
                            std::hypot(static_cast<float>(transform.a),
                                       static_cast<float>(transform.b)));
                        const float sy = std::max(0.001F,
                            std::hypot(static_cast<float>(transform.c),
                                       static_cast<float>(transform.d)));
                        const float px = static_cast<float>((transform.d * dx - transform.c * dy) / det) * sx;
                        const float py = static_cast<float>((transform.a * dy - transform.b * dx) / det) * sy;
                        constexpr float kExpandHitSlop = 10.0F;
                        if (px >= layout->expandButton.left - kExpandHitSlop &&
                            px <= layout->expandButton.right + kExpandHitSlop &&
                            py >= layout->expandButton.top - kExpandHitSlop &&
                            py <= layout->expandButton.bottom + kExpandHitSlop) {
                            shapeExpandPointerKeys_.insert(sample.key);
                            (void)expandShapeLabel(parent->id);
                            return true;
                        }
                    }
                }
            }
        }
        const auto* shape = std::get_if<semantic::ShapeContent>(&candidate.content);
        if (candidate.kind != semantic::ObjectKind::kShape || !shape || !shape->label ||
            !textWritable(candidate.id)) continue;
        const auto selected = active ? interaction_->textSession()->read().token.object
                                     : selection_.primary();
        if (candidate.id != selected) continue;
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
    if (const auto* current = semanticObjects_.find(record->id)) {
        if(!historySceneReady())return {};
        const auto* world=sceneCoordinator_->runtimeScene().find(current->id);
        if(!world)return {};
        record->transform=world->transform;
    } else if(record->placement.parent_id) {
        const auto* parent=sceneCoordinator_->runtimeScene().find(*record->placement.parent_id);
        if(!parent||!historySceneReady())return {};
        record->transform=scene::composeTransform(parent->transform,record->transform);
    }
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
    if (!historySceneReady() || !interaction_->textSession() || !textLayoutService_ || !surface_.available)
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
    const semantic::StickyContent* sticky = nullptr;
    if (!shape && record->placement.parent_id) {
        if (const auto* parent = semanticObjects_.find(*record->placement.parent_id))
            sticky = std::get_if<semantic::StickyContent>(&parent->content);
    }
    const auto geometry = shape
        ? textLayoutService_->shapeEditingGeometry(*record, start, length)
        : sticky != nullptr
            ? textLayoutService_->stickyEditingGeometry(
                  record->id, *content, *sticky, start, length, std::nullopt,
                  [&]() {
                    if (record->placement.parent_id) {
                      if (const auto* parent = semanticObjects_.find(*record->placement.parent_id))
                        if(const auto* world=sceneCoordinator_->runtimeScene().find(parent->id))return world->transform;
                    }
                    return semantic::Transform2D{};
                  }())
            : textLayoutService_->editingGeometry(record->id, *content, start, length);
    auto adjustedGeometry = geometry;
    out.available = adjustedGeometry.available;
    out.diagnostics = adjustedGeometry.diagnostics;
    out.token = {state.token,
                 target,
                 cameraGeneration(),
                     adjustedGeometry.layoutIdentity + ":" + std::to_string(start) + ":" +
                     std::to_string(length)};
    const auto view = viewportController_->state();
    const auto t = record->transform;
    const bool stickyLabel = !shape && sticky != nullptr;
    const float labelScaleX = (shape || stickyLabel)
        ? std::max(0.001F, std::hypot(static_cast<float>(t.a), static_cast<float>(t.b))) : 1.0F;
    const float labelScaleY = (shape || stickyLabel)
        ? std::max(0.001F, std::hypot(static_cast<float>(t.c), static_cast<float>(t.d))) : 1.0F;
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
        out.caret = quad(adjustedGeometry.caret);
        for (const auto& r : adjustedGeometry.selection)
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
        } else if (const auto record = textPresentation()) {
            const auto point = viewToContent(x, y);
            bool inside = insideShape(*record, {point.first, point.second});
            if (const auto* content = std::get_if<semantic::RichTextContent>(&record->content)) {
                const auto layout = textLayoutService_->resolve(record->id, *content);
                const auto bounds = scene::computeBounds(*record, layout.get()).world;
                inside = point.first >= bounds.left && point.first <= bounds.right &&
                         point.second >= bounds.top && point.second <= bounds.bottom;
            }
            if (!inside || state.composition) {
                // Consume this click. Finishing must not create a new draft;
                // active native preedit waits for the real platform result.
                (void)editor->finish();
                textSelectionPointer_ = {};
            } else if (const auto hit = textHit(*record, sample.x, sample.y)) {
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

void CanvasEditingRuntime::resetTouchTextTap() noexcept {
    touchTapContacts_.clear();
    previousTouchTap_.reset();
    touchTapMultiContact_ = false;
}

bool CanvasEditingRuntime::beginShapeTextEditAt(float x, float y,
                                                semantic::ObjectId expected) {
    auto* editor = interaction_->textSession();
    if (!editor || expected.isZero() || selection_.primary() != expected ||
        !textWritable(expected) || hitTextAtView(x, y) != expected)
        return false;
    const auto* record = semanticObjects_.find(expected);
    if (!record || (record->kind != semantic::ObjectKind::kShape &&
                    record->kind != semantic::ObjectKind::kSticky))
        return false;

    runtime::TextIntent edit;
    edit.kind = runtime::TextIntentKind::kEdit;
    edit.object = expected;
    edit.viewX = x;
    edit.viewY = y;
    if (editor->intent(edit).status != runtime::TextStatus::kApplied)
        return false;

    const auto state = editor->read();
    if (const auto presentation = textPresentation()) {
        if (const auto hit = textHit(*presentation, x, y)) {
            runtime::TextInputEvent selection;
            selection.token = state.token;
            selection.sequence = state.nextInputSequence;
            selection.kind = runtime::TextInputKind::kSelection;
            selection.range = {*hit, 0};
            (void)editor->input(selection);
        }
    }
    textSelectionPointer_ = {};
    canonicalPresentationDirty_ = true;
    return true;
}

bool CanvasEditingRuntime::observeTouchTextTap(const input::PointerSample& sample) {
    using input::PointerPhase;
    using input::PointerTool;
    constexpr std::uint64_t kTapDurationNs = 350'000'000U;
    constexpr std::uint64_t kDoubleTapIntervalNs = 500'000'000U;
    constexpr float kMoveSlop = 24.0F;
    constexpr float kDoubleTapSlop = 36.0F;

    if (sample.capabilities.tool != PointerTool::kFinger || !sample.key.valid())
        return false;
    if (sample.phase == PointerPhase::kDown) {
        if (!touchTapContacts_.empty()) {
            // A multi-contact sequence is navigation/selection input, never a
            // text double tap. Discard every candidate until all fingers lift.
            touchTapMultiContact_ = true;
            previousTouchTap_.reset();
            for (auto& [key, contact] : touchTapContacts_) {
                (void)key;
                contact.moved = true;
            }
        }
        const auto object = hitTextAtView(sample.x, sample.y);
        const auto* record = object.isZero() ? nullptr : semanticObjects_.find(object);
        touchTapContacts_.insert_or_assign(
            sample.key, TouchTapContact{record && (record->kind == semantic::ObjectKind::kShape ||
                                                   record->kind == semantic::ObjectKind::kSticky)
                                            ? object : semantic::ObjectId{},
                                        sample.x, sample.y, sample.timestampNs, false});
        return false;
    }

    const auto contact = touchTapContacts_.find(sample.key);
    if (contact == touchTapContacts_.end())
        return false;
    if (sample.phase == PointerPhase::kMove) {
        const auto dx = sample.x - contact->second.x;
        const auto dy = sample.y - contact->second.y;
        if (dx * dx + dy * dy > kMoveSlop * kMoveSlop)
            contact->second.moved = true;
        return false;
    }
    if (sample.phase != PointerPhase::kUp && sample.phase != PointerPhase::kCancel)
        return false;

    const auto current = contact->second;
    touchTapContacts_.erase(contact);
    const auto upDx = sample.x - current.x;
    const auto upDy = sample.y - current.y;
    const bool movedOnRelease = upDx * upDx + upDy * upDy > kMoveSlop * kMoveSlop;
    if (sample.phase == PointerPhase::kCancel || touchTapMultiContact_ || current.moved ||
        movedOnRelease ||
        current.object.isZero() || sample.timestampNs < current.timestampNs ||
        sample.timestampNs - current.timestampNs > kTapDurationNs) {
        previousTouchTap_.reset();
        if (touchTapContacts_.empty()) touchTapMultiContact_ = false;
        return false;
    }

    const bool isDoubleTap = previousTouchTap_ &&
        previousTouchTap_->key.source == sample.key.source &&
        previousTouchTap_->key != sample.key &&
        previousTouchTap_->object == current.object &&
        sample.timestampNs >= previousTouchTap_->timestampNs &&
        sample.timestampNs - previousTouchTap_->timestampNs <= kDoubleTapIntervalNs &&
        (sample.x - previousTouchTap_->x) * (sample.x - previousTouchTap_->x) +
                (sample.y - previousTouchTap_->y) * (sample.y - previousTouchTap_->y) <=
            kDoubleTapSlop * kDoubleTapSlop &&
        selection_.primary() == current.object;
    if (isDoubleTap) {
        previousTouchTap_.reset();
        if (touchTapContacts_.empty()) touchTapMultiContact_ = false;
        return beginShapeTextEditAt(sample.x, sample.y, current.object);
    }

    previousTouchTap_ = CompletedTouchTap{sample.key, current.object, sample.x, sample.y,
                                          sample.timestampNs};
    if (touchTapContacts_.empty()) touchTapMultiContact_ = false;
    return false;
}
} // namespace canvas::interaction
