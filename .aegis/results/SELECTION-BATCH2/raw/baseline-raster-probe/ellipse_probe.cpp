#include "C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas/runtime/render/src/skia_geometry_internal.hpp"

#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
#include <iostream>
int main(){
 SkPathBuilder b;b.addOval(SkRect::MakeWH(80,60));auto oval=b.detach();
 canvas::semantic::SolidStroke style{{.1F,.2F,.3F,1},2,canvas::semantic::StrokeCap::kButt,canvas::semantic::MiterJoin{4},canvas::semantic::SolidDash{}};
 auto border=skpathutils::FillPathWithPaint(oval,canvas::render::internal::geometryStrokePaint(style));auto fill=Op(oval,border,kUnion_SkPathOp);
 SkPathBuilder r(SkPathFillType::kEvenOdd);r.addRect(SkRect::MakeLTRB(-140,20,70,55));auto region=r.detach();
 SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(2e-4F);p.setStrokeJoin(SkPaint::kRound_Join);
 auto edge=skpathutils::FillPathWithPaint(region,p);auto expanded=Op(region,edge,kUnion_SkPathOp);
 auto raw=Op(*fill,region,kIntersect_SkPathOp);auto inflated=Op(*fill,*expanded,kIntersect_SkPathOp);
 SkPathBuilder rb;rb.addRect(SkRect::MakeWH(100,100));auto rr=rb.detach();auto rp=canvas::render::internal::geometryStrokePaint(style);
 auto frame=skpathutils::FillPathWithPaint(rr,rp);auto uf=Op(SkPath{},frame,kUnion_SkPathOp);
 SkPathBuilder sb(SkPathFillType::kEvenOdd);sb.addRect(SkRect::MakeLTRB(45,45,49,49));auto sr=sb.detach();auto se=skpathutils::FillPathWithPaint(sr,p);
 auto i1=Op(*uf,sr,kIntersect_SkPathOp),i2=Op(*uf,se,kIntersect_SkPathOp);
 std::cout<<"frameContains="<<frame.contains(47,47)<<" unionContains="<<uf->contains(47,47)<<" sr="<<sr.contains(47,47)<<" edgeOutside="<<se.contains(1,1)<<" edgeCenter="<<se.contains(47,47)<<" rawRectEmpty="<<i1->isEmpty()<<" contactRectEmpty="<<i2->isEmpty()<<"\n";
 std::cout<<"oval="<<oval.contains(40,30)<<" border="<<border.contains(40,30)<<" fill="<<fill->contains(40,30)<<" region="<<region.contains(40,30)<<" expanded="<<expanded->contains(40,30)<<" rawEmpty="<<raw->isEmpty()<<" inflatedEmpty="<<inflated->isEmpty()<<"\n";
}
