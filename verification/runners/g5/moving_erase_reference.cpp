// Offline expected-state adapter only. Link to the frozen old libraries;
// rasterization remains the separately pinned uncached CPU oracle executable.
#include "canvas/semantic/snapshot.hpp"
#include "../../../runtime/interaction/src/partial_erase_geometry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace canvas;
int main(int argc,char** argv){try{
    if(argc!=4)throw std::runtime_error("baseline snapshot, prior-traces JSON, output snapshot required");
    std::ifstream in(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};
    auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
    if(!decoded.ok()||decoded.snapshot->objects.size()!=19)throw std::runtime_error("reference baseline must decode19 objects");
    std::ifstream req(argv[2]);nlohmann::json request;req>>request;
    unsigned gesture=0;
    for(const auto& points:request.at("history")){
        std::vector<foundation::WorldPoint> trace;
        for(const auto& point:points)trace.push_back({point[0].get<float>(),point[1].get<float>()});
        unsigned object=0;
        for(auto& record:decoded.snapshot->objects){
            if(!std::holds_alternative<semantic::CompactStrokeContent>(record.content))throw std::runtime_error("nonink reference object");
            auto mask=interaction::partialEraseGeometry(trace,record.transform,16.);
            if(!mask)throw std::runtime_error("invalid fixed reference trace");
            record.erase_masks.push_back({foundation::ObjectId::fromUint64(0xa11ce0000ULL+gesture*256+object),std::move(*mask)});++object;
        }
        ++gesture;
    }
    if(!semantic::SnapshotCodec::bindDigests(*decoded.snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1))throw std::runtime_error("reference digest binding");
    const auto encoded=semantic::SnapshotCodec::encode(*decoded.snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
    if(!encoded.ok())throw std::runtime_error("expected-state snapshot encoding");
    std::ofstream out(argv[3],std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());
    if(!out)throw std::runtime_error("expected-state snapshot write");
    std::cout<<nlohmann::json{{"status","REFERENCE_SNAPSHOT"},{"objects",19},{"prior_masks_per_object",gesture},{"bytes",encoded.bytes.size()},{"authority","old partialEraseGeometry + original snapshot content; no candidate raster/cache"}}.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
