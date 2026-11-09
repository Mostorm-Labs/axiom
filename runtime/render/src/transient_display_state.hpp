#pragma once
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/editing_overlay.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"

namespace canvas::render::internal {
// One disposable attempt, owned by the existing renderer. It never owns a
// document or a successful display. Reuse only after actual GPU completion;
// snapshots still referenced by submitted work must not cause uncharged COW.
struct TransientDisplaySurface final {
    sk_sp<SkSurface> surface;
    resources::ResourceLease pixels,metadata;
    std::uint64_t epoch=0;
};
inline void drawTransientChrome(SkCanvas& canvas,const FramePlan& plan,const EditingOverlay& overlay){
    if(const auto& hover=overlay.creationHover()){
        SkPaint ghost;ghost.setAlphaf(.25F);canvas.saveLayer(nullptr,&ghost);
        if(hover->sibling)drawTransientObjectToSkCanvas(canvas,plan,*hover->sibling);
        drawTransientObjectToSkCanvas(canvas,plan,hover->primary,hover->route?&*hover->route:nullptr);canvas.restore();
    }
    drawEditingOverlayToSkCanvas(canvas,plan.frame.metrics,overlay);
}
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS) || defined(AXIOM_G5_UR0_DIAGNOSTICS)
// Private native/qualification diagnostics, absent from the normal consumer.
// No SDK command exposes these faults or returns this budget owner.
struct TransientDisplayFault final {
    int beforeObject=-1,afterTile=-1;
    bool beforePublish=false;
};
inline thread_local TransientDisplayFault transientDisplayFault;
inline thread_local std::weak_ptr<resources::ResourceBudgetManager> transientDisplayBudget;
inline thread_local std::weak_ptr<TransientDisplaySurface> transientDisplayAttempt;
inline bool transientObjectRefused(){auto& n=transientDisplayFault.beforeObject;return n>=0&&n--==0;}
inline bool transientTileRefused(){auto& n=transientDisplayFault.afterTile;return n>=0&&n--==0;}
inline bool transientPublicationRefused(){const auto refuse=transientDisplayFault.beforePublish;transientDisplayFault.beforePublish=false;return refuse;}
#else
inline bool transientObjectRefused(){return false;}
inline bool transientTileRefused(){return false;}
inline bool transientPublicationRefused(){return false;}
#endif
}
