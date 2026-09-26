#include "canvas/ink/brush_package.hpp"
#include <cassert>
#include <string>
int main(){
  const std::string m=R"({"schemaVersion":1,"packageId":"0123456789abcdef0123456789abcdef","revision":1,"pipeline":"pipeline.json","resources":[]})";
  const std::string p=R"({"pipelineVersion":1,"defaultsVersion":1,"profile":"vector-solid-v1","vector":{"size":20,"thinning":0,"smoothing":0.25,"streamline":0.75,"pressureSource":"device","missingPressure":"half","startCap":false,"endCap":true},"paint":{"rgba":[0,0.1,0.2,1],"opacity":0.5}})";
  const auto good=canvas::ink::parseBrushPackage(m,p); assert(good); assert(good.package.vector.size==20); assert(good.package.vector.pressureSource==canvas::ink::BrushPressureSource::kDevice); assert(good.canonical==canvas::ink::parseBrushPackage(m,p).canonical);
  const auto bad=canvas::ink::parseBrushPackage(m,R"({"pipelineVersion":2,"defaultsVersion":1,"profile":"vector-solid-v1"})"); assert(!bad);
  const auto unsupported=canvas::ink::parseBrushPackage(m,R"({"pipelineVersion":1,"defaultsVersion":1,"profile":"vector-solid-v1","stages":{"shape":{"mode":"on"}}})"); assert(!unsupported);
  return 0;
}
