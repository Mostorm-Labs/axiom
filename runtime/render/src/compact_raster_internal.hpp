#pragma once
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/render/render_cache.hpp"
#include "include/core/SkPicture.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkSurface.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "include/core/SkRegion.h"
#include "render_execution.hpp"
#include <array>
#include <functional>
#include "source_retention.hpp"
#include "canvas/foundation/internal/frame_timing.hpp"

namespace canvas::render::internal {
class SkiaInkLayers;
inline thread_local std::size_t* pressureCommandCounter=nullptr;
struct PressureCommandScope final {
    std::size_t* previous=pressureCommandCounter;
    explicit PressureCommandScope(std::size_t* value){pressureCommandCounter=value;}
    ~PressureCommandScope(){pressureCommandCounter=previous;}
};
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS)
inline thread_local std::uint64_t pressureTestCommands=0;
#endif
#if defined(AXIOM_ENABLE_FRAME_TIMING)
// Diagnostic-only attribution of procedural enqueue versus later flush APIs.
// The ordinary SDK compiles out this state and every update.
struct PressureDiagnosticWork final {
    std::uint64_t directCalls=0,directKnots=0,rangeCalls=0,visited=0,queued=0,enqueueNs=0;
};
inline thread_local PressureDiagnosticWork pressureDiagnosticWork;
struct PressureDiagnosticScope final {
    const char* role;std::uint64_t started=foundation::internal::frame_timing::now();
    PressureDiagnosticWork before=pressureDiagnosticWork;
    explicit PressureDiagnosticScope(const char* value):role(value){}
    ~PressureDiagnosticScope(){const auto& w=pressureDiagnosticWork;
        std::fprintf(stderr,"AXIOM_PRESSURE_WORK {\"role\":\"%s\",\"start_ns\":%.0f,\"end_ns\":%.0f,\"direct_calls\":%.0f,\"direct_knots\":%.0f,\"range_calls\":%.0f,\"visited\":%.0f,\"queued\":%.0f,\"enqueue_ns\":%.0f}\n",
            role,double(started),double(foundation::internal::frame_timing::now()),double(w.directCalls-before.directCalls),
            double(w.directKnots-before.directKnots),double(w.rangeCalls-before.rangeCalls),double(w.visited-before.visited),
            double(w.queued-before.queued),double(w.enqueueNs-before.enqueueNs));}
};
#endif
// Render-owner temporary resources only. Eight slots cover GPU/CPU full/edge
// tile extents (up to 2 MiB before backend COW/driver residency);
// snapshots keep Skia's copy-on-write semantics, never borrowed mutable pixels.
class CompactCoverageTargets final {
 public:
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    SkiaInkLayers* retainedPressure=nullptr;
    resources::ResourceLease detach(const SkSurface*);
    bool owns(const SkSurface* value)const noexcept{return std::any_of(slots_.begin(),slots_.end(),[&](const auto& s){return s.surface.get()==value;});}
    void setValidationPressure(ValidationPressure pressure){pressure_=std::move(pressure);}
    sk_sp<SkSurface> acquire(SkCanvas&,int width,int height,bool compatible,CompactRasterWork*,bool scalar=false);
    struct Allocation final {
        sk_sp<SkSurface> surface;
        ExecutionRoute outcome=ExecutionRoute::kRejected;
        ExecutionReason reason=ExecutionReason::kInvalidGeometry;
    };
    Allocation acquire(SkCanvas&,int,int,const ExecutionDecision&,CompactRasterWork*,bool scalar=false);
    void clear() noexcept {for(auto& slot:slots_)slot=Slot{};}
 private:
    struct Slot {sk_sp<SkSurface> surface;resources::ResourceLease lease;bool scalar=false;const void* context=nullptr;ExecutionBasis basis{};std::uint64_t used=0;};
    std::array<Slot,8> slots_{};
    ValidationPressure pressure_;
    std::uint64_t tick_=0;
};
// One device tile at a time, max-combined before material is applied once.
// No pointer to a FramePlan survives a yield. The owner validates source and
// target revisions before resuming with the current immutable knot span.
class CompactStrokeRasterJob final {
 public:
    CompactStrokeRasterJob(SkCanvas&,const foundation::InkBrushConfig&,
        std::span<const foundation::InkKnot>,const CompactDeviceGrid* = nullptr,bool compatible=false,
        std::shared_ptr<resources::ResourceBudgetManager> budget=nullptr,
        std::size_t maxRecordedBytes=RenderCacheOptions{}.maxEntryBytes,
        std::function<void(std::size_t)> reclaim={},bool canonicalReplay=false,std::shared_ptr<const PreparedNavigationGeometry> prepared=nullptr,bool directConsumer=false,bool whiteOnly=false);
    bool step(SkCanvas&,const foundation::InkBrushConfig&,std::span<const foundation::InkKnot>,
        std::uint64_t seed,CompactCoverageTargets&,bool compatible,CompactRasterWork*,std::size_t* pressureCommands=nullptr);
    [[nodiscard]] bool resourcePending()const noexcept{return resourcePending_;}
    void setCaptureOnly()noexcept{captureOnly_=true;}
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    void diagnosticCursor(std::uint64_t& primitive,std::uint64_t& knot,int& x,int& y,bool& done)const noexcept {
        primitive=preparedPrimitive_;knot=knot_;x=x_;y=y_;done=done_;
    }
#endif
 private:
    std::shared_ptr<const PreparedNavigationGeometry> prepared_;
    std::size_t preparedPrimitive_=0;
    bool directConsumer_=false,whiteOnly_=false,captureOnly_=false;
    SkMatrix matrix_{};
    SkIRect clip_{},deviceClip_{};
    SkRect support_{};
    SkRegion interior_{},tileInterior_{};
    resources::ResourceLease interiorLease_;
    sk_sp<SkSurface> correction_;
    bool hybrid_=false;
    bool canCull_=false,done_=false,resourcePending_=false;
    int x_=0,y_=0;
    std::size_t knot_=0;
    double distance_=0,totalDistance_=0;
    std::optional<ink::CompactContourCompiler> contour_;
    sk_sp<SkSurface> tile_;
    resources::ResourceLease tileLease_; // Exclusive suspended capture, never a pooled borrow.
    struct ContourRecord {
        sk_sp<SkPicture> picture;resources::ResourceLease lease;SkRect bounds{};
        std::optional<ink::CompactContourCompiler> checkpoint;
        bool empty=true;
    };
    struct RecordGroup {SkRect bounds{};std::size_t end=0;};
    static constexpr std::size_t recordsPerGroup=16,selectionChecksPerStep=32;
    std::vector<ContourRecord> contourRecords_;
    std::vector<RecordGroup> recordGroups_;
    std::shared_ptr<resources::ResourceBudgetManager> recordBudget_;
    resources::ResourceLease recordStorageLease_;
    SkRect recordBounds_{};
    std::size_t recordLimit_=0,recordBytes_=0,recordStorageBytes_=0;
    bool recordComplete_=false,picturesEnabled_=false;
    bool canonicalReplay_=false;
    SkBlendMode contourBlend_=SkBlendMode::kLighten;
    std::function<void(std::size_t)> reclaim_;
    void recordContour(const foundation::InkBrushConfig&,const ink::CompactContour&,ink::CompactContourCompiler);
    void appendRecord(ContourRecord);
    void abandonRecords() noexcept;
    void abandonPictures() noexcept;
};
}
