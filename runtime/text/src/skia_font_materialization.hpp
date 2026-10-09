#pragma once
// Private SDK adapter, shared only with the private Skia text renderer.
#include "canvas/text/text_layout.hpp"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_data.h"
#include <algorithm>
namespace canvas::text::internal {
inline sk_sp<SkTypeface> exactFace(const FontObservation& f) {
    auto data = SkData::MakeWithCopy(f.bytes->data(),f.bytes->size());
    auto manager = SkFontMgr_New_Custom_Data({&data,1});
    auto face = manager ? manager->makeFromData(data,f.collectionIndex) : nullptr;
    if (!face) return nullptr;
    if (face->fontStyle().weight() != static_cast<int>(f.weight)) {
        const auto count = face->getVariationDesignParameters({});
        if (count <= 0) return nullptr;
        std::vector<SkFontParameters::Variation::Axis> axes(static_cast<std::size_t>(count));
        face->getVariationDesignParameters(axes);
        const auto wght = SkSetFourByteTag('w','g','h','t');
        const auto axis = std::find_if(axes.begin(),axes.end(),[&](const auto& a){return a.tag==wght;});
        if (axis==axes.end() || f.weight < axis->min || f.weight > axis->max) return nullptr;
        SkFontArguments::VariationPosition::Coordinate coordinate{wght,static_cast<float>(f.weight)};
        SkFontArguments args;
        args.setCollectionIndex(f.collectionIndex).setVariationDesignPosition({&coordinate,1})
            .setSyntheticBold(false).setSyntheticOblique(false);
        face = face->makeClone(args);
    }
    if (!face || face->fontStyle().weight()!=static_cast<int>(f.weight) ||
        face->fontStyle().slant() != (f.italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant) ||
        face->isSyntheticBold() || face->isSyntheticOblique()) return nullptr;
    return face;
}

}
