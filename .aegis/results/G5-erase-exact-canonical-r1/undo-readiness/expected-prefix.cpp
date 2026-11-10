#include "canvas/semantic/snapshot.hpp"
#include "../../../../runtime/interaction/src/partial_erase_geometry.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace canvas;
int main(int argc,char**argv){try{std::ifstream f(argv[1],std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!d.ok())return 2;std::vector<foundation::WorldPoint> trace{{300,230},{300,242},{300,245},{300,248},{300,251},{300,254}};
for(auto&o:d.snapshot->objects)if(std::holds_alternative<semantic::CompactStrokeContent>(o.content)){auto mask=interaction::partialEraseGeometry(trace,o.transform,16);if(!mask)return 3;foundation::ObjectId id;id.bytes.fill(0xff);o.erase_masks.push_back({id,*mask});std::sort(o.erase_masks.begin(),o.erase_masks.end(),[](const auto&a,const auto&b){return a.id<b.id;});}
if(!semantic::SnapshotCodec::bindDigests(*d.snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1))return 4;auto e=semantic::SnapshotCodec::encode(*d.snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!e.ok())return 5;std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(e.bytes.data()),e.bytes.size());std::cout<<"expected prefix encoded "<<e.bytes.size()<<std::endl;return 0;}catch(...){return 6;}}
