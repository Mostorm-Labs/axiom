#pragma once
#include "source_retention.hpp"
#include <array>
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"

namespace canvas::render::internal {
struct MaskHistorySourceCharge;
struct MaskHistoryUsage {
    std::size_t bytes=0;
    // Private fixed-size storage keeps identical layout across test/library
    // translation units. Ordinary builds compile out all category updates.
    std::array<std::size_t,7> categories{};
    std::map<const SourceRetention*,std::weak_ptr<MaskHistorySourceCharge>> sources;
};
// One local charge for a source allocation shared by draw and exact views.
// The existing SourceRetention owns its single global reservation.
struct MaskHistorySourceCharge final {
    std::shared_ptr<const SourceRetention> source;
    resources::ResourceLease metadata;
    std::shared_ptr<MaskHistoryUsage> usage;
    std::size_t bytes=0;
    ~MaskHistorySourceCharge(){if(usage){usage->bytes-=bytes;usage->sources.erase(source.get());
#if defined(AXIOM_UR2D_TESTS)
        usage->categories[0]-=bytes;
#endif
    }}
};
struct PreparedMaskPrimitive final {
    SkPath path;
    SkRect bounds{};
    float width=0;
    bool swept=false;
    resources::ResourceLease lease;
    std::shared_ptr<MaskHistoryUsage> usage;
    std::size_t bytes=0;
    ~PreparedMaskPrimitive(){if(usage){usage->bytes-=bytes;
#if defined(AXIOM_UR2D_TESTS)
        usage->categories[1]-=bytes;
#endif
    }}
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
    std::shared_ptr<MaskHistorySourceCharge> localSource;
    resources::ResourceLease metadata;
    std::shared_ptr<MaskHistoryUsage> usage;
    std::size_t bytes=0,prefixPrimitives=0;
    std::uint64_t identity=0,prefixIdentity=0,used=0;
    ~MaskHistoryPlan(){if(usage){usage->bytes-=bytes;
#if defined(AXIOM_UR2D_TESTS)
        usage->categories[2]-=bytes;
#endif
    }}
};
}
