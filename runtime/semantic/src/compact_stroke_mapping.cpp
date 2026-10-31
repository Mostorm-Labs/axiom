#include "compact_stroke_mapping.hpp"
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include <bit>
#include <string>
namespace canvas::semantic::internal {
namespace {
void append(std::string& bytes,std::uint64_t word,unsigned width) {
    for(unsigned i=0;i<width;++i)bytes.push_back(static_cast<char>(word>>(8*i)));
}
template<class Bytes>
std::uint64_t word(const Bytes& bytes,std::size_t offset,unsigned width) {
    std::uint64_t result=0;
    for(unsigned i=0;i<width;++i)result|=std::uint64_t(static_cast<unsigned char>(bytes[offset+i]))<<(8*i);
    return result;
}
}
void setCompactStroke(const CompactStrokeContent& source,auditoryworks::axiom::v1::CompactStrokeContent& out) {
    const auto& s=source.stroke;const auto& c=s.config;auto& d=*out.mutable_config();
    d.set_version(c.version);d.set_algorithm_version(c.algorithmVersion);d.set_signal_version(c.signalVersion);
    d.set_material_version(c.materialVersion);d.set_family(static_cast<unsigned>(c.family));
    d.set_round_mode(static_cast<unsigned>(c.roundMode));d.set_pressure_source(static_cast<unsigned>(c.pressureSource));
    for(double color:c.color)d.add_paint(color);
    d.set_size(c.size);d.set_opacity(c.opacity);d.set_marker_angle(c.markerAngle);d.set_marker_flatness(c.markerFlatness);
    d.set_density(c.density);d.set_texture_scale(c.textureScale);d.set_deposition(c.deposition);d.set_wet_edge(c.wetEdge);
    d.set_material_resource_id(c.materialResourceId);d.set_material_resource_sha256(c.materialResourceSha256);d.set_material_sampling_version(c.materialSamplingVersion);
    d.set_laser_fade_ms(c.laserFadeMs);d.set_laser_tail_length(c.laserTailLength);
    out.set_seed(s.seed);out.set_knot_count(static_cast<std::uint32_t>(s.knots.size()));
    std::string xy,p;xy.reserve(s.knots.size()*16);p.reserve(s.knots.size()*2);
    for(const auto& k:s.knots){append(xy,std::bit_cast<std::uint64_t>(k.x),8);append(xy,std::bit_cast<std::uint64_t>(k.y),8);append(p,k.pressure,2);}
    out.set_positions_le(xy);out.set_effective_pressure_le(p);
}
bool getCompactStroke(const auditoryworks::axiom::v1::CompactStrokeContent& in,CompactStrokeContent& out) {
    if(!in.has_config()||!in.has_seed()||!in.has_knot_count()||!in.has_positions_le()||!in.has_effective_pressure_le()||
       in.knot_count()==0||in.knot_count()>262144||in.positions_le().size()!=std::size_t(in.knot_count())*16||
       in.effective_pressure_le().size()!=std::size_t(in.knot_count())*2)return false;
    const auto& d=in.config();
    if(!d.has_version()||!d.has_algorithm_version()||!d.has_signal_version()||!d.has_material_version()||
       !d.has_family()||!d.has_round_mode()||!d.has_pressure_source()||d.paint_size()!=4||!d.has_size()||
       !d.has_opacity()||!d.has_marker_angle()||!d.has_marker_flatness()||!d.has_density()||!d.has_texture_scale()||
        !d.has_deposition()||!d.has_wet_edge()||!d.has_material_resource_id()||!d.has_material_resource_sha256()||!d.has_material_sampling_version())return false;
    auto& s=out.stroke;auto& c=s.config;
    c.version=d.version();c.algorithmVersion=d.algorithm_version();c.signalVersion=d.signal_version();
    c.materialVersion=d.material_version();c.family=static_cast<foundation::InkFamily>(d.family());
    c.roundMode=static_cast<foundation::InkRoundMode>(d.round_mode());c.pressureSource=static_cast<foundation::InkPressureSource>(d.pressure_source());
    for(int i=0;i<4;++i)c.color[i]=d.paint(i);
    c.size=d.size();c.opacity=d.opacity();c.markerAngle=d.marker_angle();c.markerFlatness=d.marker_flatness();
    c.density=d.density();c.textureScale=d.texture_scale();c.deposition=d.deposition();c.wetEdge=d.wet_edge();
    c.materialResourceId=d.material_resource_id();c.materialResourceSha256=d.material_resource_sha256();c.materialSamplingVersion=d.material_sampling_version();
    if(d.has_laser_fade_ms())c.laserFadeMs=d.laser_fade_ms();
    if(d.has_laser_tail_length())c.laserTailLength=d.laser_tail_length();
    if(!foundation::validInkBrushConfig(c,true))return false;
    s.seed=in.seed();s.knots.clear();s.knots.reserve(in.knot_count());
    for(std::size_t i=0;i<in.knot_count();++i) {
        const auto x=std::bit_cast<double>(word(in.positions_le(),i*16,8));
        const auto y=std::bit_cast<double>(word(in.positions_le(),i*16+8,8));
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        s.knots.push_back({x==0?0:x,y==0?0:y,static_cast<std::uint16_t>(word(in.effective_pressure_le(),i*2,2))});
    }
    return validCompactStroke(s);
}
}
#endif
