#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/validator.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::interaction {
namespace {
using namespace semantic;
using namespace runtime;
bool eligible(ObjectKind kind) {
    return kind == ObjectKind::kShape || kind == ObjectKind::kSticky ||
           kind == ObjectKind::kConnector || kind == ObjectKind::kVectorPath ||
           kind == ObjectKind::kSection || kind == ObjectKind::kTable;
}
bool radiusEligible(const ObjectRecord &r) {
    if (r.kind == ObjectKind::kConnector)
        return true;
    const auto *shape = std::get_if<ShapeContent>(&r.content);
    return r.kind == ObjectKind::kShape && shape &&
           (shape->shape_kind == 1 || shape->shape_kind == 3);
}
const PropertyValue *property(const ObjectRecord &r, std::uint32_t id) {
    for (const auto &p : r.properties.entries)
        if (p.field_id == id)
            return &p.value;
    return nullptr;
}
AppearanceValues values(const ObjectRecord &r) {
    AppearanceValues v;
    if (r.kind == ObjectKind::kVectorPath && !property(r, 0x102U)) {
        v.fillKind = AppearanceFill::kSolid;
        v.fill = {0, 0, 1, 1};
    }
    if (r.kind == ObjectKind::kSection) {
        v.strokeEnabled = true;
        v.stroke = {.4F, .45F, .5F, .65F};
        v.strokeWidth = 1;
    }
    if (const auto *table = std::get_if<TableContent>(&r.content)) {
        v.strokeEnabled = table->grid.width > 0;
        v.stroke = table->grid.color;
        v.strokeWidth = table->grid.width;
    }
    if (r.kind == ObjectKind::kSticky) {
        v.fillKind = AppearanceFill::kSolid;
        v.fill = {1, 1, .5F, 1};
    }
    if (r.kind == ObjectKind::kConnector) {
        v.strokeEnabled = true;
        v.stroke = {1, 128.F / 255, 0, 1};
        v.strokeWidth = 4;
    }
    for (const auto &e : r.properties.entries) {
        if (e.field_id == 3)
            if (const auto *x = std::get_if<float>(&e.value))
                v.opacity = *x;
        if (const auto *x = std::get_if<CornerStyleV1>(&e.value))
            v.cornerRadius = x->radius;
        if (const auto *x = std::get_if<SketchStyleV1>(&e.value)) {
            v.enabled = x->enabled;
            v.seed = x->seed;
            v.roughness = x->roughness;
        }
        if (const auto *x = std::get_if<FillStyleValue>(&e.value)) {
            v.fillKind = AppearanceFill::kNone;
            if (const auto *f = std::get_if<SolidFill>(x)) {
                v.fillKind = AppearanceFill::kSolid;
                v.fill = f->color;
            }
            if (const auto *f = std::get_if<PatternFillV1>(x)) {
                v.fillKind = f->kind == PatternKind::kCrosshatch ? AppearanceFill::kCrosshatch
                                                                 : AppearanceFill::kDiagonal;
                v.fill = f->color;
                v.angle = f->angle;
                v.gap = f->gap;
                v.hatchWidth = f->width;
            }
        }
        if (const auto *x = std::get_if<StrokeStyleValue>(&e.value)) {
            v.strokeEnabled = false;
            if (const auto *s = std::get_if<SolidStroke>(x)) {
                v.strokeEnabled = true;
                v.stroke = s->color;
                v.strokeWidth = s->width;
                if (const auto *d = std::get_if<DashPattern>(&s->dash)) {
                    v.dash = d->segments;
                    v.dashOffset = d->offset;
                }
            }
        }
    }
    return v;
}
bool routingWidthChanged(const ObjectRecord &a, const ObjectRecord &b) {
    // Appearance patches cannot change cap/join, transforms or endpoint semantics.
    // Only stroke enable/width changes the existing router's clearance inputs.
    if (a.kind != ObjectKind::kShape && a.kind != ObjectKind::kSticky &&
        a.kind != ObjectKind::kConnector)
        return false;
    const auto before = values(a), after = values(b);
    return (a.kind == ObjectKind::kShape && before.cornerRadius != after.cornerRadius) ||
           before.strokeEnabled != after.strokeEnabled ||
           (before.strokeEnabled && before.strokeWidth != after.strokeWidth);
}
void set(ObjectRecord &r, std::uint32_t id, PropertyValue v) {
    for (auto &e : r.properties.entries)
        if (e.field_id == id) {
            e.value = std::move(v);
            return;
        }
    r.properties.entries.push_back({id, std::move(v)});
}
bool patched(ObjectRecord &r, const AppearancePatch &p, std::uint64_t seed) {
    const auto original = r.properties;
    auto v = values(r);
    const auto originalValues = v;
    const bool legacyPath = r.kind == ObjectKind::kVectorPath && !property(r, 0x102U);
    const bool hasFill = r.kind != ObjectKind::kConnector && r.kind != ObjectKind::kTable;
    const bool hasStroke = r.kind != ObjectKind::kTable;
    if (p.cornerRadius && radiusEligible(r))
        set(r, 0x103U, CornerStyleV1{1, *p.cornerRadius});
    // Per-kind parameters on a mixed selection are explicitly inapplicable.
    if (p.opacity)
        set(r, 3U, static_cast<float>(*p.opacity));
    if (p.enabled || p.roughness || p.reseed) {
        const auto *old = property(r, 0x102);
        SketchStyleV1 s{1, seed, 1, true};
        if (old)
            s = std::get<SketchStyleV1>(*old);
        if (p.reseed)
            s.seed = seed;
        if (p.enabled)
            s.enabled = *p.enabled;
        if (p.roughness) {
            s.roughness = *p.roughness;
            if (!p.enabled)
                s.enabled = true;
        }
        if (old || s.enabled || p.roughness || p.reseed)
            set(r, 0x102U, s);
    }
    if (hasFill && (p.angle || p.gap || p.hatchWidth) &&
        p.fillKind.value_or(v.fillKind) != AppearanceFill::kDiagonal &&
        p.fillKind.value_or(v.fillKind) != AppearanceFill::kCrosshatch)
        return false;
    if (hasFill && (p.fillKind || p.fill || p.angle || p.gap || p.hatchWidth)) {
        if (p.fillKind)
            v.fillKind = *p.fillKind;
        if (p.fill)
            v.fill = *p.fill;
        if (p.angle)
            v.angle = *p.angle;
        if (p.gap)
            v.gap = *p.gap;
        if (p.hatchWidth)
            v.hatchWidth = *p.hatchWidth;
        if (v.fillKind == AppearanceFill::kNone)
            set(r, 0x100U, FillStyleValue{NoFill{}});
        else if (v.fillKind == AppearanceFill::kSolid)
            set(r, 0x100U, FillStyleValue{SolidFill{v.fill}});
        else if (v.fillKind == AppearanceFill::kDiagonal ||
                 v.fillKind == AppearanceFill::kCrosshatch)
            set(r, 0x100U,
                FillStyleValue{PatternFillV1{1, v.fill,
                                             v.fillKind == AppearanceFill::kCrosshatch
                                                 ? PatternKind::kCrosshatch
                                                 : PatternKind::kDiagonal,
                                             v.angle, v.gap, v.hatchWidth}});
        else
            return false;
    }
    if (hasStroke && (p.strokeEnabled || p.stroke || p.strokeWidth || p.dash || p.dashOffset)) {
        SolidStroke s{v.stroke, v.strokeWidth, StrokeCap::kButt, MiterJoin{4}, SolidDash{}};
        if (const auto *old = property(r, 0x101U))
            if (const auto *solid = std::get_if<SolidStroke>(&std::get<StrokeStyleValue>(*old)))
                s = *solid;
        if (p.stroke)
            s.color = *p.stroke;
        if (p.strokeWidth)
            s.width = *p.strokeWidth;
        if (p.dash) {
            if (p.dash->empty())
                s.dash = SolidDash{};
            else
                s.dash = DashPattern{*p.dash, p.dashOffset.value_or(v.dashOffset)};
        } else if (p.dashOffset)
            if (auto *d = std::get_if<DashPattern>(&s.dash))
                d->offset = *p.dashOffset;
        if (p.strokeEnabled)
            v.strokeEnabled = *p.strokeEnabled;
        set(r, 0x101U, v.strokeEnabled ? StrokeStyleValue{s} : StrokeStyleValue{NoStroke{}});
    }
    if (legacyPath && values(r) != originalValues) {
        if (!property(r, 0x102U))
            set(r, 0x102U, SketchStyleV1{1, seed, v.roughness, false});
        if (!property(r, 0x100U) && originalValues.fillKind == AppearanceFill::kSolid)
            set(r, 0x100U, FillStyleValue{SolidFill{originalValues.fill}});
    }
    Operation candidate{OperationId{ObjectId::fromUint64(1)}, DocumentId{ObjectId::fromUint64(1)},
                        1, 1, InsertObjectsOp{{r}}};
    auto n = normalizeOperation(candidate);
    if (!n.ok())
        return false;
    auto &normalized = std::get<InsertObjectsOp>(n.value.payload).objects.front();
    if ((normalized.kind == ObjectKind::kSticky || normalized.kind == ObjectKind::kSection) &&
        normalized.properties != original && !property(normalized, 0x102U))
        set(normalized, 0x102U, SketchStyleV1{1, seed, v.roughness, false});
    if (!validatePayloadStructure(n.value).ok())
        return false;
    r = std::move(normalized);
    return true;
}
bool patchValid(const AppearancePatch &p) {
    const auto number = [](const auto &x, double lo, double hi) {
        return !x || (std::isfinite(*x) && *x >= lo && *x <= hi);
    };
    const auto color = [](const auto &c) {
        return !c || (std::isfinite(c->r) && std::isfinite(c->g) && std::isfinite(c->b) &&
                      std::isfinite(c->a) && c->r >= 0 && c->r <= 1 && c->g >= 0 && c->g <= 1 &&
                      c->b >= 0 && c->b <= 1 && c->a >= 0 && c->a <= 1);
    };
    if (!number(p.cornerRadius, 0, 16384) || !number(p.roughness, 0, 3) ||
        !number(p.angle, -180, 180) || !number(p.gap, 2, 1024) || !number(p.hatchWidth, .1, 64) ||
        !number(p.strokeWidth, .01, 1024) || !number(p.opacity, 0, 1) || !color(p.fill) ||
        !color(p.stroke) || !number(p.dashOffset, -1e9, 1e9))
        return false;
    if (p.fillKind && static_cast<unsigned>(*p.fillKind) > 3)
        return false;
    if (p.dash) {
        if (p.dash->size() > 64 || p.dash->size() % 2 != 0)
            return false;
        for (double n : *p.dash)
            if (!std::isfinite(n) || n <= 0 || n > 1e6)
                return false;
    }
    return true;
}
void mixed(AppearanceSnapshot &out, const AppearanceValues &b, bool fill, bool stroke) {
    const auto &a = out.values;
#define M(field)                                                                                   \
    if (a.field != b.field)                                                                        \
    out.mixed.insert(#field)
    M(enabled);
    M(roughness);
    if (fill) {
        M(fillKind);
        M(fill);
        M(angle);
        M(gap);
        M(hatchWidth);
    }
    if (stroke) {
        M(strokeEnabled);
        M(stroke);
        M(strokeWidth);
        M(dash);
        M(dashOffset);
    }
    M(opacity);

#undef M
}
} // namespace
std::uint64_t CanvasEditingRuntime::allocateSketchSeed() {
    const auto id = allocateTextIdentity();
    std::uint64_t seed = 1469598103934665603ULL;
    for (auto b : id.bytes) {
        seed ^= b;
        seed *= 1099511628211ULL;
    }
    return id.isZero() ? 0 : seed ? seed : 1;
}
runtime::AppearanceValues CanvasEditingRuntime::appearanceDefaults() const {
    auto v = futureStyle_;
    v.seed = 0;
    v.fill = {futureAppearance_.fill.r, futureAppearance_.fill.g, futureAppearance_.fill.b,
              futureAppearance_.fill.a};
    v.stroke = {futureAppearance_.stroke.r, futureAppearance_.stroke.g, futureAppearance_.stroke.b,
                futureAppearance_.stroke.a};
    v.strokeWidth = futureAppearance_.strokeWidth;
    v.opacity = futureAppearance_.opacity;
    v.strokeEnabled = futureAppearance_.strokeEnabled;
    if (!futureAppearance_.fillEnabled)
        v.fillKind = runtime::AppearanceFill::kNone;
    return v;
}
void CanvasEditingRuntime::materializeAppearance(semantic::PropertyBag &bag, bool fill, bool sticky,
                                                 bool rounded) {
    auto v = appearanceDefaults();
    semantic::ObjectRecord record;
    record.properties = bag;
    if (record.properties.entries.empty() && futureStyleConfigured_) {
        set(record, 3U, static_cast<float>(v.opacity));
        if (fill)
            set(record, 0x100U,
                v.fillKind == runtime::AppearanceFill::kNone
                    ? semantic::FillStyleValue{semantic::NoFill{}}
                    : semantic::FillStyleValue{semantic::SolidFill{v.fill}});
        set(record, 0x101U,
            v.strokeEnabled ? semantic::StrokeStyleValue{semantic::SolidStroke{
                                  v.stroke, v.strokeWidth, semantic::StrokeCap::kButt,
                                  semantic::MiterJoin{4}, semantic::SolidDash{}}}
                            : semantic::StrokeStyleValue{semantic::NoStroke{}});
    }
    if (v.enabled || (sticky && futureStyleConfigured_))
        record.properties.entries.push_back(
            {0x102U, semantic::SketchStyleV1{1, allocateSketchSeed(), v.roughness, v.enabled}});
    if (fill && (v.fillKind == runtime::AppearanceFill::kDiagonal ||
                 v.fillKind == runtime::AppearanceFill::kCrosshatch)) {
        set(record, 0x100U,
            semantic::FillStyleValue{
                semantic::PatternFillV1{1, v.fill,
                                        v.fillKind == runtime::AppearanceFill::kCrosshatch
                                            ? semantic::PatternKind::kCrosshatch
                                            : semantic::PatternKind::kDiagonal,
                                        v.angle, v.gap, v.hatchWidth}});
    }
    if (!v.dash.empty())
        for (auto &e : record.properties.entries)
            if (e.field_id == 0x101U)
                if (auto *s = std::get_if<semantic::StrokeStyleValue>(&e.value))
                    if (auto *solid = std::get_if<semantic::SolidStroke>(s))
                        solid->dash = semantic::DashPattern{v.dash, v.dashOffset};
    if (rounded && v.cornerRadius > 0)
        set(record, 0x103U, CornerStyleV1{1, v.cornerRadius});
    bag = std::move(record.properties);
}
runtime::AppearanceValues
CanvasEditingRuntime::appearanceValuesFor(const semantic::ObjectRecord &record) const {
    return values(record);
}
void CanvasEditingRuntime::materializeStructuralAppearance(semantic::ObjectRecord &record) {
    const auto v = appearanceDefaults();
    if (v.opacity != 1)
        set(record, 3U, static_cast<float>(v.opacity));
    if (v.enabled)
        set(record, 0x102U, semantic::SketchStyleV1{1, allocateSketchSeed(), v.roughness, true});
}
runtime::AppearanceSnapshot
CanvasEditingRuntime::appearanceSnapshot(const runtime::MultiSelectionProjection &projection,
                                         bool defaults) const {
    using namespace runtime;
    AppearanceSnapshot out;
    if (std::this_thread::get_id() != ownerThread_ || closed_ || !historySceneReady())
        return out;
    if (defaults) {
        out.available = true;
        out.writable = true;
        out.values = appearanceDefaults();
        return out;
    }
    if (!projection.available)
        return out;
    out.available = true;
    out.stamp = projection.stamp;
    out.writable = multiSelectionPermissionAllowed_ && textReady() && !explicitIntentActive_ &&
                   !creationActive() && !selectionTransformActive();
    if (auto *editor = interaction_->textSession();
        editor && editor->read().phase != TextPhase::kClosed)
        out.writable = false;
    for (auto id : projection.roots) {
        const auto *record = semanticObjects_.find(id);
        if (!record) {
            return {};
        }
        if (!eligible(record->kind)) {
            out.excluded.push_back(id);
            out.fillExcluded.push_back(id);
            out.strokeExcluded.push_back(id);
            out.radiusExcluded.push_back(id);
            continue;
        }
        out.eligible.push_back(id);
        out.writable = out.writable && textWritable(id);
        auto v = values(*record);
        if (out.objects.empty())
            out.values = v;
        else
            mixed(out, v,
                  record->kind != ObjectKind::kConnector && record->kind != ObjectKind::kTable &&
                      !out.fillEligible.empty(),
                  record->kind != ObjectKind::kTable && !out.strokeEligible.empty());
        if (record->kind == ObjectKind::kConnector || record->kind == ObjectKind::kTable)
            out.fillExcluded.push_back(id);
        else {
            if (out.fillEligible.empty()) {
                out.values.fillKind = v.fillKind;
                out.values.fill = v.fill;
                out.values.angle = v.angle;
                out.values.gap = v.gap;
                out.values.hatchWidth = v.hatchWidth;
            }
            out.fillEligible.push_back(id);
        }
        if (record->kind == ObjectKind::kTable)
            out.strokeExcluded.push_back(id);
        else {
            if (out.strokeEligible.empty()) {
                out.values.stroke = v.stroke;
                out.values.strokeEnabled = v.strokeEnabled;
                out.values.strokeWidth = v.strokeWidth;
                out.values.dash = v.dash;
                out.values.dashOffset = v.dashOffset;
            }
            out.strokeEligible.push_back(id);
        }
        if (radiusEligible(*record)) {
            if (out.radiusEligible.empty())
                out.values.cornerRadius = v.cornerRadius;
            else if (out.values.cornerRadius != v.cornerRadius)
                out.mixed.insert("cornerRadius");
            out.radiusEligible.push_back(id);
        } else
            out.radiusExcluded.push_back(id);
        out.objects.push_back({id, v});
    }
    if (out.eligible.empty())
        out.writable = false;
    if (out.objects.size() != 1)
        out.values.seed = 0;
    return out;
}
bool CanvasEditingRuntime::appearanceSessionCurrent() const {
    if (!appearanceSession_)
        return false;
    const auto &t = appearanceSession_->token.stamp;
    const auto current = selectionSummary();
    return !closed_ && multiSelectionPermissionAllowed_ &&
           t.permissionRevision == multiSelectionPermissionRevision_ &&
           t.selectionRevision == current.selectionRevision &&
           t.semanticGeneration == current.semanticGeneration &&
           t.sceneGeneration == current.sceneGeneration && t.sceneRevision == current.sceneRevision;
}
void CanvasEditingRuntime::clearAppearancePreview() {
    if (appearanceSession_) {
        appearanceSession_.reset();
        ++appearancePreviewRevision_;
        canonicalPresentationDirty_ = true;
    }
}
runtime::AppearanceResult
CanvasEditingRuntime::appearanceCommand(const runtime::AppearanceRequest &r,
                                        const runtime::MultiSelectionProjection &projection) {
    using namespace runtime;
    using namespace semantic;
    AppearanceResult out;
    out.generation = semanticGeneration_.current().value();
    const auto fail = [&](AppearanceError e) {
        clearAppearancePreview();
        out.error = e;
        return out;
    };
    if (std::this_thread::get_id() != ownerThread_ || closed_ || !historySceneReady())
        return out;
    if (!patchValid(r.patch))
        return fail(AppearanceError::kInvalid);
    if (r.action == AppearanceAction::kDefaults) {
        if (appearanceSession_)
            return fail(AppearanceError::kBusy);
        if (r.patch.reseed)
            return fail(AppearanceError::kInvalid);
        ObjectRecord fake;
        fake.id = ObjectId::fromUint64(1);
        fake.kind = ObjectKind::kShape;
        fake.kind_version = 1;
        fake.placement.order_key = OrderKey({128});
        fake.content = ShapeContent{1, 120, 80};
        auto v = appearanceDefaults();
        set(fake, 3U, static_cast<float>(v.opacity));
        set(fake, 0x100U,
            v.fillKind == AppearanceFill::kNone ? FillStyleValue{NoFill{}}
            : v.fillKind == AppearanceFill::kSolid
                ? FillStyleValue{SolidFill{v.fill}}
                : FillStyleValue{PatternFillV1{1, v.fill,
                                               v.fillKind == AppearanceFill::kCrosshatch
                                                   ? PatternKind::kCrosshatch
                                                   : PatternKind::kDiagonal,
                                               v.angle, v.gap, v.hatchWidth}});
        set(fake, 0x101U,
            v.strokeEnabled ? StrokeStyleValue{SolidStroke{
                                  v.stroke, v.strokeWidth, StrokeCap::kButt, MiterJoin{4},
                                  v.dash.empty() ? StrokeDash{SolidDash{}}
                                                 : StrokeDash{DashPattern{v.dash, v.dashOffset}}}}
                            : StrokeStyleValue{NoStroke{}});
        set(fake, 0x102U, SketchStyleV1{1, 1, v.roughness, v.enabled});
        if (v.cornerRadius > 0)
            set(fake, 0x103U, CornerStyleV1{1, v.cornerRadius});
        const auto prior = v;
        auto effective = r.patch;
        effective.angle.reset();
        effective.gap.reset();
        effective.hatchWidth.reset();
        if (!patched(fake, effective, 1))
            return fail(AppearanceError::kInvalid);
        v = values(fake);
        v.fill = r.patch.fill.value_or(prior.fill);
        v.stroke = r.patch.stroke.value_or(prior.stroke);
        v.strokeWidth = r.patch.strokeWidth.value_or(prior.strokeWidth);
        v.dash = r.patch.dash.value_or(prior.dash);
        v.dashOffset = r.patch.dashOffset.value_or(prior.dashOffset);
        v.angle = r.patch.angle.value_or(prior.angle);
        v.gap = r.patch.gap.value_or(prior.gap);
        v.hatchWidth = r.patch.hatchWidth.value_or(prior.hatchWidth);
        v.seed = 0;
        if (v == prior) {
            out.applied = true;
            out.error = AppearanceError::kNone;
            return out;
        }
        v.seed = 0;
        futureStyle_ = v;
        futureAppearance_ = {v.fillKind != AppearanceFill::kNone,
                             {v.fill.r, v.fill.g, v.fill.b, v.fill.a},
                             v.strokeEnabled,
                             {v.stroke.r, v.stroke.g, v.stroke.b, v.stroke.a},
                             static_cast<float>(v.strokeWidth),
                             static_cast<float>(v.opacity)};
        futureStyleConfigured_ = true;
        out.applied = true;
        out.error = AppearanceError::kNone;
        return out;
    }
    const bool sessionAction = r.action == AppearanceAction::kPreview ||
                               r.action == AppearanceAction::kCommit ||
                               r.action == AppearanceAction::kCancel;
    if (sessionAction) {
        if (!appearanceSession_ || r.token != appearanceSession_->token || !projection.available ||
            projection.stamp != r.token.stamp || !appearanceSessionCurrent())
            return fail(AppearanceError::kStale);
        out.token = r.token;
        if (r.action == AppearanceAction::kCancel) {
            clearAppearancePreview();
            out.applied = true;
            out.error = AppearanceError::kNone;
            return out;
        }
    }
    auto observation = appearanceSnapshot(projection, false);
    if ((r.patch.cornerRadius && observation.radiusEligible.empty()) ||
        ((r.patch.fillKind || r.patch.fill || r.patch.angle || r.patch.gap || r.patch.hatchWidth) &&
         observation.fillEligible.empty()) ||
        ((r.patch.strokeEnabled || r.patch.stroke || r.patch.strokeWidth || r.patch.dash ||
          r.patch.dashOffset) &&
         observation.strokeEligible.empty()))
        return fail(AppearanceError::kInvalid);
    if (!observation.writable || !canonicalWriteAdmission() || dispatchRuntime_.activeCount() != 0)
        return fail(AppearanceError::kUnavailable);
    if (!sessionAction && (r.stamp != observation.stamp || r.eligible != observation.eligible))
        return fail(AppearanceError::kStale);
    if (!sessionAction && appearanceSession_)
        return fail(AppearanceError::kBusy);
    AppearanceSession local;
    if (!sessionAction) {
        local.token = {observation.stamp, ++appearanceSerial_};
        for (auto id : observation.eligible) {
            local.before.push_back(*semanticObjects_.find(id));
            local.seeds.push_back(allocateSketchSeed());
        }
        local.after = local.before;
    }
    auto &session = sessionAction ? *appearanceSession_ : local;
    if (r.action == AppearanceAction::kBegin) {
        appearanceSession_ = std::move(local);
        ++appearancePreviewRevision_;
        out.token = appearanceSession_->token;
        out.applied = true;
        out.error = AppearanceError::kNone;
        return out;
    }
    if (r.action == AppearanceAction::kPatch || r.action == AppearanceAction::kPreview) {
        auto after = session.after;
        for (std::size_t i = 0; i < after.size(); ++i) {
            if (!patched(after[i], r.patch, session.seeds[i]))
                return fail(AppearanceError::kInvalid);
        }
        for (std::size_t i = 0; i < after.size(); ++i)
            if (routingWidthChanged(session.after[i], after[i])) {
                ++appearancePreviewRevision_;
                break;
            }
        session.after = std::move(after);
        session.routingChanged = false;
        for (std::size_t i = 0; i < session.after.size(); ++i)
            session.routingChanged =
                session.routingChanged || routingWidthChanged(session.before[i], session.after[i]);
    }
    if (r.action == AppearanceAction::kPreview) {
        canonicalPresentationDirty_ = true;
        out.applied = true;
        out.error = AppearanceError::kNone;
        out.token = session.token;
        return out;
    }
    if (r.action != AppearanceAction::kPatch && r.action != AppearanceAction::kCommit)
        return fail(AppearanceError::kInvalid);
    PatchPropertiesOp forward, inverse;
    for (std::size_t i = 0; i < session.before.size(); ++i) {
        const auto &before = session.before[i];
        const auto &after = session.after[i];
        if (!semanticObjects_.find(before.id) || *semanticObjects_.find(before.id) != before)
            return fail(AppearanceError::kStale);
        std::set<std::uint32_t> fields;
        for (const auto &e : before.properties.entries)
            fields.insert(e.field_id);
        for (const auto &e : after.properties.entries)
            fields.insert(e.field_id);
        for (auto field : fields) {
            const auto *a = property(before, field), *b = property(after, field);
            if ((!a && !b) || (a && b && *a == *b))
                continue;
            forward.patches.push_back({before.id, field,
                                       b ? PropertyPatchAction::kSet : PropertyPatchAction::kClear,
                                       b ? std::variant<std::monostate, PropertyValue>{*b}
                                         : std::variant<std::monostate, PropertyValue>{}});
            inverse.patches.push_back({before.id, field,
                                       a ? PropertyPatchAction::kSet : PropertyPatchAction::kClear,
                                       a ? std::variant<std::monostate, PropertyValue>{*a}
                                         : std::variant<std::monostate, PropertyValue>{}});
        }
    }
    if (forward.patches.empty()) {
        clearAppearancePreview();
        out.applied = true;
        out.error = AppearanceError::kNone;
        return out;
    }
    const auto f = allocateOperationId(), u = allocateOperationId();
    clearAppearancePreview();
    if (f.isZero() || u.isZero() ||
        !applyLocalHistoryOperation({f, documentId_, 1, 1, std::move(forward)},
                                    {u, documentId_, 1, 1, std::move(inverse)}))
        return fail(AppearanceError::kRejected);
    out.applied = true;
    out.changed = true;
    out.error = AppearanceError::kNone;
    out.generation = semanticGeneration_.current().value();
    return out;
}
bool CanvasEditingRuntime::applyAppearanceToRecord(canvas::RuntimeSceneRecord &record) {
    if (!appearanceSession_ || !appearanceSessionCurrent())
        return false;
    const auto preview =
        std::find_if(appearanceSession_->after.begin(), appearanceSession_->after.end(),
                     [&](const auto &object) { return object.id == record.objectId; });
    const bool connector = record.kind == semantic::ObjectKind::kConnector;
    if (preview == appearanceSession_->after.end() && !connector)
        return false;
    const auto *source = preview != appearanceSession_->after.end()
                             ? &*preview
                             : semanticObjects_.find(record.objectId);
    if (!source)
        return false;
    auto object = *source;
    object.transform = record.transform;
    record.properties = object.properties;
    std::optional<scene::ConnectorGeometry> route;
    if (connector) {
        if (appearanceSession_->routingChanged)
            route = previewConnectorGeometry(object);
        else if (const auto *canonical =
                     std::get_if<scene::ConnectorGeometry>(&record.derivedGeometry))
            route = *canonical;
    }
    if (route)
        record.derivedGeometry = *route;
    const auto bounds =
        scene::computeBounds(object, record.textLayout.get(), route ? &*route : nullptr);
    record.geometryBounds = bounds.geometry;
    record.visualBounds = bounds.visual;
    record.worldBounds = bounds.world;
    return true;
}
void CanvasEditingRuntime::applyAppearancePreview(render::ReferenceDrawList &list) {
    if (appearanceSession_ && !appearanceSessionCurrent())
        clearAppearancePreview();
    if (!appearanceSession_)
        return;
    for (auto &entry : list.entries)
        applyAppearanceToRecord(entry.record);
    list.canonicalBytes.clear();
    list.digest.clear();
}
} // namespace canvas::interaction
