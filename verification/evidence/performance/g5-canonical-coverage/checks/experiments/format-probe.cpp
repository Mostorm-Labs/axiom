#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include <cstdio>
#include <vector>
int main(){
 for(auto type:{kN32_SkColorType,kGray_8_SkColorType,kR8_unorm_SkColorType}){
  const auto info=SkImageInfo::Make(256,256,type,kOpaque_SkAlphaType);
  auto surface=SkSurfaces::Raster(info);std::printf("type=%d drawable=%d\n",int(type),bool(surface));
  if(!surface)continue;
  auto* c=surface->getCanvas();c->clear(SK_ColorBLACK);SkPaint p;p.setAntiAlias(true);p.setBlendMode(SkBlendMode::kLighten);p.setColor(SK_ColorWHITE);c->drawCircle(128,128,80,p);
  std::vector<unsigned> pixels(256*256);auto out=SkImageInfo::MakeN32Premul(256,256);
  auto ok=surface->readPixels(out,pixels.data(),256*4,0,0);std::printf("read=%d black=%x white=%x\n",ok,pixels[0],pixels[128*256+128]);
 }
}
