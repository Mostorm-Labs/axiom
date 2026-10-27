#include "canvas/interaction/region_selection_geometry.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/text/text_layout.hpp"
#include <gtest/gtest.h>
using namespace canvas;
using namespace canvas::interaction;
namespace {
std::vector<RegionPoint> box(double x,double y,double w=4,double h=4) {return {{x,y},{x+w,y},{x+w,y+h},{x,y+h}};}
semantic::ObjectRecord object(semantic::ObjectKind kind,semantic::ObjectContent content) {
    semantic::ObjectRecord r;r.id=foundation::ObjectId::fromUint64(1);r.kind=kind;r.kind_version=1;r.content=std::move(content);return r;
}
TEST(SelectionCoverage, VectorPathCurveAndEvenOddHoleUseGeometryNotControlBounds) {
    semantic::VectorPathContent c;c.geometry.commands={semantic::MoveTo{{0,0}},semantic::QuadTo{{50,100},{100,0}},semantic::ClosePath{}};
    auto r=object(semantic::ObjectKind::kVectorPath,c);
    EXPECT_EQ(intersectSelectionRegion(r,box(48,30)),RegionIntersection::kHit);
    EXPECT_EQ(intersectSelectionRegion(r,box(48,80)),RegionIntersection::kMiss);
    c.geometry.fill_rule=semantic::FillRule::kEvenOdd;
    c.geometry.commands={semantic::MoveTo{{0,0}},semantic::LineTo{{100,0}},semantic::LineTo{{100,100}},semantic::LineTo{{0,100}},semantic::ClosePath{},
        semantic::MoveTo{{20,20}},semantic::LineTo{{80,20}},semantic::LineTo{{80,80}},semantic::LineTo{{20,80}},semantic::ClosePath{}};
    r.content=c;EXPECT_EQ(intersectSelectionRegion(r,box(40,40)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(5,5)),RegionIntersection::kHit);
}
TEST(SelectionCoverage, LegacyVectorStrokeUsesFiniteWidthSegmentsNotLargeAabb) {
    semantic::VectorStrokeContent c;c.stroke.brush.nominal_size=10;c.stroke.brush.opacity=1;c.stroke.brush.color.a=1;
    c.stroke.data=semantic::VectorStrokeData{{{{0,0},1,{}},{{100,100},1,{}}}};
    const auto r=object(semantic::ObjectKind::kVectorStroke,c);
    EXPECT_EQ(intersectSelectionRegion(r,box(48,48)),RegionIntersection::kHit);
    EXPECT_EQ(intersectSelectionRegion(r,box(0,90)),RegionIntersection::kMiss);
}
TEST(SelectionCoverage, CanonicalBrushOutlineAndFilledEraseHoleFollowVisibleGeometry) {
    semantic::BrushStrokeContent c;c.stroke.snapshot.paint.alpha=1;c.stroke.snapshot.paint.opacity=1;
    c.stroke.vector_output.outline={{0,0},{100,0},{100,100},{0,100}};
    auto r=object(semantic::ObjectKind::kVectorStroke,c);r.kind_version=2;
    semantic::FilledPathMask mask;mask.path.commands={semantic::MoveTo{{20,20}},semantic::LineTo{{80,20}},semantic::LineTo{{80,80}},semantic::LineTo{{20,80}},semantic::ClosePath{}};
    r.erase_masks={{foundation::ObjectId::fromUint64(2),mask}};
    EXPECT_EQ(intersectSelectionRegion(r,box(40,40)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(5,5)),RegionIntersection::kHit);
    r.transform={0,1,-1,0,200,0};EXPECT_EQ(intersectSelectionRegion(r,box(150,40)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(190,5)),RegionIntersection::kHit);
}
TEST(SelectionCoverage, SweptEraseUsesTheRendererMaxRadiusCubicStroke) {
    semantic::BrushStrokeContent c;c.stroke.snapshot.paint.alpha=1;c.stroke.snapshot.paint.opacity=1;
    c.stroke.vector_output.outline={{0,0},{100,0},{100,100},{0,100}};
    auto r=object(semantic::ObjectKind::kVectorStroke,c);r.kind_version=2;
    semantic::SweptCircleMask mask;mask.segments={{{{0,50},10},{{100,50},10},{30,50},{70,50}}};
    r.erase_masks={{foundation::ObjectId::fromUint64(2),mask}};
    EXPECT_EQ(intersectSelectionRegion(r,box(45,45)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(45,25)),RegionIntersection::kHit);
}
TEST(SelectionCoverage, LegacyDabsAreSeparateSquaresAndSolidBrushDabsAreDisks) {
    semantic::DabStrokeContent c;c.stroke.brush.opacity=1;c.stroke.brush.color.a=1;
    c.stroke.data=semantic::DabStrokeData{{{{20,20},10,0,1},{{80,80},10,0,1}}};
    auto r=object(semantic::ObjectKind::kDabStroke,c);
    EXPECT_EQ(intersectSelectionRegion(r,box(24,24,.5,.5)),RegionIntersection::kHit);
    EXPECT_EQ(intersectSelectionRegion(r,box(48,48)),RegionIntersection::kMiss);
    semantic::DabBrushStrokeContent d;d.stroke.snapshot.paint.alpha=1;d.stroke.snapshot.paint.opacity=1;d.stroke.snapshot.material_mode=1;
    d.stroke.dab_output=std::get<semantic::DabStrokeData>(c.stroke.data);r.kind_version=2;r.content=d;
    EXPECT_EQ(intersectSelectionRegion(r,box(24,24,.5,.5)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(19,19)),RegionIntersection::kHit);
    d.stroke.snapshot.material_mode=4;r.content=d;
    EXPECT_EQ(intersectSelectionRegion(r,box(19,19)),RegionIntersection::kUnsupported);
}
TEST(SelectionCoverage, ConnectorUsesPublishedRouteAndArrowheadAndUnavailableIsExplicit) {
    semantic::ConnectorContent c;c.start.value=semantic::FreePointEndpoint{{0,0}};c.end.value=semantic::FreePointEndpoint{{100,100}};
    auto r=object(semantic::ObjectKind::kConnector,c);
    RuntimeSceneRecord p;p.objectId=r.id;p.kind=r.kind;p.content=c;
    scene::ConnectorGeometry g{{0,0},{100,100},{{0,0},{0,100},{100,100}},true};p.derivedGeometry=g;
    EXPECT_EQ(intersectSelectionRegion(r,box(-1,48,2,4),&p),RegionIntersection::kHit);
    EXPECT_EQ(intersectSelectionRegion(r,box(48,48),&p),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(89,103,1,1),&p),RegionIntersection::kHit);
    g.status=runtime::ConnectorRouteStatus::kPending;p.derivedGeometry=g;
    EXPECT_EQ(intersectSelectionRegion(r,box(48,48),&p),RegionIntersection::kUnsupported);
}
TEST(SelectionCoverage, ImageFitLetterboxAndCroppedOutSourceAreNotVisibleContent) {
    semantic::ImageContent c;c.width=100;c.height=100;c.intrinsic_width=200;c.intrinsic_height=100;c.content_mode=semantic::ImageContentMode::kFit;
    auto r=object(semantic::ObjectKind::kImage,c);
    EXPECT_EQ(intersectSelectionRegion(r,box(40,5)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(40,45)),RegionIntersection::kHit);
    c.source_rect=semantic::NormalizedRect{.25,0,.5,1};r.content=c;
    EXPECT_EQ(intersectSelectionRegion(r,box(-40,40)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(40,5)),RegionIntersection::kHit);
}
TEST(SelectionCoverage, RichTextUsesExistingLineFragmentsAndMissingLayoutIsUnsupported) {
    auto r=object(semantic::ObjectKind::kRichText,semantic::RichTextContent{});
    RuntimeSceneRecord p;p.objectId=r.id;p.kind=r.kind;
    auto l=std::make_shared<text::TextLayoutSnapshot>();
    l->lines={{{},0,1,{0,0,30,10},8},{{},1,2,{0,30,30,40},38}};p.textLayout=l;
    EXPECT_EQ(intersectSelectionRegion(r,box(5,3),&p),RegionIntersection::kHit);
    EXPECT_EQ(intersectSelectionRegion(r,box(5,18),&p),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(50,3),&p),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(5,3)),RegionIntersection::kUnsupported);
    semantic::RichTextContent text;text.document.paragraphs={{{}, {}, {{"visible text",{}}}}};r.content=text;
    p.textLayout=std::make_shared<text::TextLayoutSnapshot>();
    EXPECT_EQ(intersectSelectionRegion(r,box(5,3),&p),RegionIntersection::kUnsupported);
}
TEST(SelectionCoverage, SectionUsesItsAuthoredBoundaryAndGroupHasNoOwnPixels) {
    auto r=object(semantic::ObjectKind::kSection,semantic::SectionContent{{10,20,100,60},"",0,0});
    EXPECT_EQ(intersectSelectionRegion(r,box(30,40)),RegionIntersection::kHit);
    EXPECT_EQ(intersectSelectionRegion(r,box(0,0)),RegionIntersection::kMiss);
    r=object(semantic::ObjectKind::kGroup,semantic::GroupContent{});
    EXPECT_EQ(intersectSelectionRegion(r,box(30,40)),RegionIntersection::kMiss);
}
TEST(SelectionCoverage, StrokeOnlyShapeHasNoSelectableInteriorButHasVisibleBorder) {
    auto r=object(semantic::ObjectKind::kShape,semantic::ShapeContent{1,100,100});
    r.properties.entries={{0x100U,semantic::FillStyleValue{semantic::NoFill{}}},
        {0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},4}}}};
    EXPECT_EQ(intersectSelectionRegion(r,box(45,45)),RegionIntersection::kMiss);
    EXPECT_EQ(intersectSelectionRegion(r,box(-1,45,1,4)),RegionIntersection::kHit);
}

}
