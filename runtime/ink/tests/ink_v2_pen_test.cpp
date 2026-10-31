#include "canvas/ink/compact_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <stdexcept>
#include <iostream>
int main(){
    using namespace canvas;
    foundation::InkBrushConfig regular;regular.roundMode=foundation::InkRoundMode::kRegular;
    const std::vector<foundation::InkKnot> knots{{10,10,0},{80,10,65535}};
    auto a=ink::compactCoverage(regular,knots);
    regular.roundMode=foundation::InkRoundMode::kHanddrawn;
    auto b=ink::compactCoverage(regular,knots);
    if(a.empty()||b.empty()||a.front().radius==b.front().radius)throw std::runtime_error("round mode did not change effective width");
    if(a.front().radius!=8||a.back().radius!=8)throw std::runtime_error("regular width varies");
    if(b.front().radius>=b.back().radius)throw std::runtime_error("effective pressure ignored");
    auto package=ink::BrushPackageCatalog{}.loadDefault("marker-compact-v1",1).package;
    const auto before=ink::brushPackageCanonicalDigest(package);
    package.compactConfig->markerAngle+=.2;
    if(ink::brushPackageCanonicalDigest(package)==before)throw std::runtime_error("package digest omits captured nib geometry");
    std::cout<<"regular radius="<<a.front().radius<<" pressure radii="<<b.front().radius<<","<<b.back().radius<<'\n';
}
