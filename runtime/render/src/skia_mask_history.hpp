#pragma once
#include "source_retention.hpp"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"

namespace canvas::render::internal {
struct MaskHistoryUsage {std::size_t bytes=0;};
struct PreparedMaskPrimitive final {
    SkPath path;
    SkRect bounds{};
    float width=0;
    bool swept=false;
    resources::ResourceLease lease;
    std::shared_ptr<MaskHistoryUsage> usage;
    std::size_t bytes=0;
    ~PreparedMaskPrimitive(){if(usage)usage->bytes-=bytes;}
    void draw(SkCanvas& canvas)const {
        SkPaint paint;paint.setBlendMode(SkBlendMode::kClear);paint.setAntiAlias(swept);
        if(swept){paint.setStyle(SkPaint::kStroke_Style);paint.setStrokeCap(SkPaint::kRound_Cap);
            paint.setStrokeJoin(SkPaint::kRound_Join);paint.setStrokeWidth(width);}
        canvas.drawPath(path,paint);
    }
};
std::shared_ptr<PreparedMaskPrimitive> makePreparedMaskPrimitive();
struct MaskHistoryPlan final {
    scene::SharedVector<semantic::EraseMaskRecord> records;
    std::vector<std::shared_ptr<const PreparedMaskPrimitive>> primitives;
    std::vector<std::size_t> primitiveEnds;
    std::vector<std::shared_ptr<const void>> exactPaths;
    std::shared_ptr<const SourceRetention> source;
    resources::ResourceLease metadata;
    std::shared_ptr<MaskHistoryUsage> usage;
    std::size_t bytes=0,prefixPrimitives=0;
    std::uint64_t identity=0,prefixIdentity=0,used=0;
    ~MaskHistoryPlan(){if(usage)usage->bytes-=bytes;}
};
}
