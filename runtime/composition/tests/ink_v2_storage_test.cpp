#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/interaction/brush_commit_adapter.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/codec.hpp"
#include <iostream>
#include <stdexcept>

using namespace canvas;
namespace {
void require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
std::vector<std::uint8_t> snapshot(const semantic::ObjectRecord* object){
    semantic::SemanticSnapshot s;s.document_id=semantic::DocumentId(semantic::ObjectId::fromUint64(123));
    s.schema_version=3;s.manifest_present=true;s.document_revision=1;s.digest_algorithm=1;
    s.recovery_frontier={8,1,16,1,24,1};if(object)s.objects={*object};
    constexpr auto capability=semantic::SnapshotCapability::kAllWithCompactStrokeV1;
    require(semantic::SnapshotCodec::populateRequirements(s)&&semantic::SnapshotCodec::bindDigests(s,capability),"snapshot binding failed");
    auto encoded=semantic::SnapshotCodec::encode(s,capability);require(encoded.ok(),"snapshot encoding failed");
    auto decoded=semantic::SnapshotCodec::decode(encoded.bytes,capability);
    require(decoded.ok()&&decoded.snapshot->objects==s.objects,"snapshot did not preserve frozen carrier");
    if(object&&std::holds_alternative<semantic::CompactStrokeContent>(object->content))
        require(!semantic::SnapshotCodec::decode(encoded.bytes,semantic::SnapshotCapability::kAllWithRoundedAndFontFallbackV1).ok(),"old reader admitted compact carrier");
    return encoded.bytes;
}
semantic::Operation stroke(const char* profile,unsigned revision,unsigned count){
    auto loaded=ink::BrushPackageCatalog{}.loadDefault(profile,revision);require(bool(loaded),"profile load failed");
    ink::BrushSession session(1,ink::resolveBrushState(loaded.package,42));require(session.begin(),"begin");
    std::vector<ink::BrushSample> input;
    for(unsigned i=0;i<count;++i)input.push_back({double(i)*8,0,.5,false,i+1,std::uint64_t(i)*8'000'000,false});
    ink::BrushPreviewDelta delta;require(session.append(input,{},delta)==ink::BrushSessionError::kNone,"append");
    ink::BrushCommitIntent intent;require(session.seal(intent)==ink::BrushSessionError::kNone,"seal");
    return interaction::BrushCommitAdapter::build(intent,loaded.package,17,semantic::DocumentId(semantic::ObjectId::fromUint64(123)));
}
}
int main(){
    const auto envelope=snapshot(nullptr).size();
    for(unsigned n:{2048U,8192U}){
        auto old=stroke("chalk-grain-v1",4,n);auto oldBytes=semantic::SemanticCodec::encodeProtobufOperation(old);
        require(oldBytes.ok(),"legacy chalk baseline encoding");const auto oldSnapshot=snapshot(&std::get<semantic::AddStrokeOp>(old.payload).object).size()-envelope;
        for(const char* profile:{"marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"})for(unsigned revision:{1U,3U}){
            if(revision==3&&(std::string_view(profile)=="marker-compact-v1"||std::string_view(profile)=="crayon-wax-v1"))continue;
            auto op=stroke(profile,revision,n);auto encoded=semantic::SemanticCodec::encodeProtobufOperation(op);require(encoded.ok(),"compact op encoding");
            const auto& object=std::get<semantic::AddStrokeOp>(op.payload).object;
            const auto& compact=std::get<semantic::CompactStrokeContent>(object.content).stroke;
            const auto bytes=snapshot(&object).size()-envelope;
            std::cerr<<"storage profile="<<profile<<" revision="<<revision<<" knots="<<compact.knots.size()<<" bytes="<<encoded.bytes.size()<<std::endl;
            require(encoded.bytes.size()<=16384+18*compact.knots.size(),"compact byte budget");
            require(encoded.bytes.size()<=oldBytes.bytes.size()*.3&&bytes<=oldSnapshot*.3,"compact did not reduce actual legacy encoding");
            // Same evaluator/config/seed, with the exact retained linear
            // segments expanded at four-world-unit intervals. Both writers
            // use real AddStroke/Snapshot codecs. No legacy dab renderer is
            // substituted for the compact material in this comparison.
            auto diagnostic=object;auto expanded=compact;expanded.knots.clear();
            for(std::size_t i=1;i<compact.knots.size();++i){const auto& a=compact.knots[i-1];const auto& b=compact.knots[i];
                const auto length=std::hypot(b.x-a.x,b.y-a.y);const auto steps=std::max(1U,unsigned(std::ceil(length/4.)));
                for(unsigned j=0;j<steps;++j){const auto t=double(j)/steps;
                    expanded.knots.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,static_cast<std::uint16_t>(std::lround(a.pressure+(double(b.pressure)-a.pressure)*t))});}
            }
            expanded.knots.push_back(compact.knots.back());diagnostic.content=semantic::CompactStrokeContent{std::move(expanded)};
            auto diagnosticOp=op;diagnosticOp.payload=semantic::AddStrokeOp{diagnostic};
            auto expandedBytes=semantic::SemanticCodec::encodeProtobufOperation(diagnosticOp);require(expandedBytes.ok(),"diagnostic expanded encoding");
            const auto expandedSnapshot=snapshot(&diagnostic).size()-envelope;
            require(encoded.bytes.size()<=expandedBytes.bytes.size()*.4&&bytes<=expandedSnapshot*.4,"compact did not reduce diagnostic encoding");
            std::cout<<"{\"profile\":\""<<profile<<"\",\"revision\":"<<revision<<",\"samples\":"<<n<<",\"knots\":"<<compact.knots.size()<<",\"operation_bytes\":"<<encoded.bytes.size()<<",\"snapshot_delta_bytes\":"<<bytes<<",\"legacy_operation_bytes\":"<<oldBytes.bytes.size()<<",\"legacy_snapshot_delta_bytes\":"<<oldSnapshot<<",\"diagnostic_operation_bytes\":"<<expandedBytes.bytes.size()<<",\"diagnostic_snapshot_delta_bytes\":"<<expandedSnapshot<<",\"resource_delta_bytes\":0,\"persisted_dabs\":0,\"persisted_raw\":0,\"persisted_outline\":0}\n";
        }
    }
}
