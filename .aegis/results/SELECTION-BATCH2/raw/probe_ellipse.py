from pathlib import Path
import json,re,subprocess
root=Path.cwd();out=root/'out/selection-batch2';probe=out/'baseline-raster-probe';build=root/'out/selection-batch1/native'
source=probe/'ellipse_probe.cpp';source.write_text('#include "'+(root/'runtime/render/src/skia_geometry_internal.hpp').as_posix()+'"\n'+r'''
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
''')
old=json.loads((probe/'commands.json').read_text())[0];command=old['command'];obj=probe/'ellipse_probe.obj';command=re.sub(r'/Fo\S+',lambda m:'/Fo'+str(obj),command);command=re.sub(r'/Fd\S+',lambda m:'/Fd'+str(probe/'ellipse_probe.pdb'),command);command=re.sub(r' -c ("[^"]+"|\S+)',lambda m:' -c "'+str(source)+'"',command)
with (probe/'ellipse_compile.log').open('w') as f:r=subprocess.run(command,shell=True,cwd=build,stdout=f,stderr=subprocess.STDOUT)
if r.returncode:print((probe/'ellipse_compile.log').read_text()[-800:]);raise SystemExit(r.returncode)
s=(build/'build.ninja').read_text().splitlines();i=next(i for i,l in enumerate(s) if l.startswith('build ') and 'axiom_render_headless_skia_tests.exe:' in l);libs=next(l.split(' = ',1)[1] for l in s[i:i+15] if l.startswith('  LINK_LIBRARIES = '))
rsp=probe/'ellipse_link.rsp';rsp.write_text('"'+str(obj)+'" '+libs);exe=probe/'ellipse_probe.exe'
with (probe/'ellipse_link.log').open('w') as f:r=subprocess.run(['link.exe','/nologo','@'+str(rsp),'/out:'+str(exe),'/machine:x64','/subsystem:console'],cwd=build,stdout=f,stderr=subprocess.STDOUT)
if r.returncode:print((probe/'ellipse_link.log').read_text()[-800:]);raise SystemExit(r.returncode)
r=subprocess.run([str(exe)],capture_output=True,text=True);(probe/'ellipse_run.log').write_text(r.stdout+r.stderr);print(r.stdout);raise SystemExit(r.returncode)
