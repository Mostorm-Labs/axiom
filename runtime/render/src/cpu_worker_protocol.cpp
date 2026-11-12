#include "cpu_worker_protocol.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
namespace canvas::render::internal {
namespace {
void u32(std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(v>>(i*8));}
void u64(std::vector<std::uint8_t>& b,std::uint64_t v){for(unsigned i=0;i<8;++i)b.push_back(v>>(i*8));}
std::uint32_t read32(std::span<const std::uint8_t>b,std::size_t p){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(b[p+i])<<(8*i);return n;}
std::uint64_t read64(std::span<const std::uint8_t>b,std::size_t p){std::uint64_t n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(b[p+i])<<(8*i);return n;}
void point(std::vector<std::uint8_t>& b,semantic::Vec2 v){u64(b,std::bit_cast<std::uint64_t>(v.x));u64(b,std::bit_cast<std::uint64_t>(v.y));}
}
std::array<std::uint8_t,16> cpuKernelId(){std::array<std::uint8_t,16>v{};const char*s=AXIOM_CPU_KERNEL_ID;const auto nib=[](char c){return c<='9'?c-'0':c-'a'+10;};for(unsigned i=0;i<16;++i)v[i]=std::uint8_t(nib(s[i*2])*16+nib(s[i*2+1]));return v;}
std::optional<std::size_t> cpuConstructionEstimate(std::size_t count,std::size_t wireBytes,bool sweep)noexcept{
 if(count>(sweep?cpuSweepLimit:cpuFilledLimit)||wireBytes>cpuInputLimit)return {};
 // 4096bytes per segment is the accepted conservative PathOps construction
 // bound. The accumulator+COW is covered by that bound; the other side is a
 // bounded single segment. Keep8MiB for linkedruntime/stack/allocator and input.
 constexpr std::size_t fixed=8U*1024U*1024U+cpuOutputLimit+65536U;
 const auto values=count*(sweep?sizeof(semantic::EraseCubicSegment):sizeof(semantic::PathCommand));
 if(values>cpuHeapBytes-fixed||wireBytes>cpuHeapBytes-fixed-values)return {};
 const auto base=fixed+values+wireBytes;
 if(count>(cpuHeapBytes-base)/4096U)return {};
 return base+count*4096U;
}
std::vector<std::uint8_t> cpuEncodeRequest(const CpuRequest& r){
 if(r.prefixCount){
  const auto* full=std::get_if<semantic::SweptCircleMask>(&r.geometry);
  const auto* witness=r.prefixWitness?std::get_if<semantic::SweptCircleMask>(&*r.prefixWitness):nullptr;
  if(!full||!witness||witness->segments.size()!=r.prefixCount||r.prefixCount>full->segments.size()||
     !std::equal(witness->segments.begin(),witness->segments.end(),full->segments.begin())||!r.prefixPath.isFinite())return {};
  CpuRequest plain;plain.identity=r.identity;plain.geometry=r.geometry;
  auto bytes=cpuEncodeRequest(plain);
  const auto n=r.prefixPath.writeToMemory(nullptr);
  if(bytes.empty()||!n||n>cpuInputLimit-bytes.size())return {};
  const auto set=[&](unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)bytes[p+i]=v>>(8*i);};
  set(4,2);set(104,r.prefixCount);set(108,n);set(112,r.prefixCount*80U);
  const auto at=bytes.size();bytes.resize(at+n);r.prefixPath.writeToMemory(bytes.data()+at);
  set(96,bytes.size()-cpuHeaderSize);return bytes;
 }
 const auto size=std::visit([](const auto& v){if constexpr(requires{v.segments;})return v.segments.size();else return v.path.commands.size();},r.geometry);
 const bool sweep=std::holds_alternative<semantic::SweptCircleMask>(r.geometry);
 if(size>(sweep?cpuSweepLimit:cpuFilledLimit))return {};
 const auto maximum=size*80U+cpuHeaderSize;if(maximum>cpuInputLimit)return {};
 std::vector<std::uint8_t> p;std::uint32_t variant=0,count=0,fill=0;
 if(const auto*s=std::get_if<semantic::SweptCircleMask>(&r.geometry)){count=s->segments.size();for(const auto&x:s->segments){point(p,x.p0.position);u64(p,std::bit_cast<std::uint64_t>(x.p0.radius));point(p,x.p1.position);u64(p,std::bit_cast<std::uint64_t>(x.p1.radius));point(p,x.control1);point(p,x.control2);}}
 else {variant=1;const auto&g=std::get<semantic::FilledPathMask>(r.geometry).path;count=g.commands.size();fill=unsigned(g.fill_rule);for(const auto&c:g.commands){u32(p,c.index());std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,semantic::MoveTo>)point(p,v.point);else if constexpr(std::is_same_v<T,semantic::LineTo>)point(p,v.end);else if constexpr(std::is_same_v<T,semantic::QuadTo>){point(p,v.control);point(p,v.end);}else if constexpr(std::is_same_v<T,semantic::CubicTo>){point(p,v.control1);point(p,v.control2);point(p,v.end);}},c);}}
 if(p.size()+cpuHeaderSize>cpuInputLimit||!cpuConstructionEstimate(count,p.size()+cpuHeaderSize,variant==0))return {};
 std::vector<std::uint8_t>b;u32(b,0x31575043);u32(b,1);u32(b,variant);u32(b,count);const auto id=cpuKernelId();b.insert(b.end(),id.begin(),id.end());for(auto n:r.identity.words)u64(b,n);u32(b,p.size());u32(b,fill);b.resize(cpuHeaderSize);b.insert(b.end(),p.begin(),p.end());return b;
}
std::optional<CpuRequest> cpuDecodeRequest(std::span<const std::uint8_t>b){
 if(b.size()<cpuHeaderSize||b.size()>cpuInputLimit||read32(b,0)!=0x31575043)return {};
 const auto version=read32(b,4);
 if(version==2){
  const auto count=read32(b,104),pathBytes=read32(b,108),prefixBytes=read32(b,112);
  if(!count||count>cpuSweepLimit||!pathBytes||read32(b,8)!=0||prefixBytes!=count*80U||read32(b,96)!=b.size()-cpuHeaderSize||
    pathBytes>b.size()-cpuHeaderSize||read32(b,12)<count)return {};
  for(unsigned i=116;i<128;++i)if(b[i])return {};
  const auto at=b.size()-pathBytes;
  if(at!=cpuHeaderSize+std::size_t(read32(b,12))*80U)return {};
  std::vector<std::uint8_t> plain(b.begin(),b.begin()+at);
  const auto set=[&](unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)plain[p+i]=v>>(8*i);};
  set(4,1);set(96,plain.size()-cpuHeaderSize);for(unsigned i=104;i<128;++i)plain[i]=0;
  auto full=cpuDecodeRequest(plain);auto path=cpuDecodePath(b.last(pathBytes));
  if(!full||!path)return {};
  const auto* f=std::get_if<semantic::SweptCircleMask>(&full->geometry);
  if(!f||count>f->segments.size()||!cpuConstructionEstimate(f->segments.size(),b.size(),true))return {};
  // Full input is encoded once as the exact prefix followed by suffix. The
  // Render owner bound this prefix's full values to a known completed path;
  // splitting here preserves that witness without a second wire copy.
  semantic::SweptCircleMask proof;proof.segments.assign(f->segments.begin(),f->segments.begin()+count);
  full->prefixCount=count;full->prefixPath=*path;full->prefixWitness=std::move(proof);return full;
 }
 if(version!=1)return {};
 const auto id=cpuKernelId();if(!std::equal(id.begin(),id.end(),b.begin()+16))return {};
 const auto variant=read32(b,8),count=read32(b,12),length=read32(b,96),fill=read32(b,100);
 if(variant>1||!cpuConstructionEstimate(count,b.size(),variant==0)||length!=b.size()-cpuHeaderSize)return {};
 for(std::size_t i=104;i<128;++i)if(b[i])return {};
 CpuRequest r;for(unsigned i=0;i<8;++i)r.identity.words[i]=read64(b,32+i*8);
 for(unsigned i:{0U,1U,4U,5U,6U,7U})if(!r.identity.words[i])return {};
 if(!r.identity.words[2]&&!r.identity.words[3])return {};
 std::size_t pos=cpuHeaderSize;
 // Preflight all lengths/opcodes/scalars before allocating any geometry.
 const auto scan=[&]()->bool{
  std::size_t p=pos;for(unsigned i=0;i<count;++i){std::size_t n=10;if(variant){if(p+4>b.size())return false;const auto op=read32(b,p);p+=4;if(op>4)return false;n=op==0||op==1?2:op==2?4:op==3?6:0;}
   if(n>(b.size()-p)/8)return false;for(std::size_t j=0;j<n;++j)if(!std::isfinite(std::bit_cast<double>(read64(b,p+j*8))))return false;
   if(!variant&&(std::bit_cast<double>(read64(b,p+16))<0||std::bit_cast<double>(read64(b,p+40))<0))return false;p+=n*8;}
  return p==b.size();};
 if((variant==0&&fill!=0)||(variant==1&&fill!=1&&fill!=2)||!scan())return {};
 const auto number=[&](){auto n=std::bit_cast<double>(read64(b,pos));pos+=8;return n;};const auto pt=[&](){const auto x=number(),y=number();return semantic::Vec2{x,y};};
 if(!variant){semantic::SweptCircleMask g;g.segments.reserve(count);for(unsigned i=0;i<count;++i){semantic::EraseCubicSegment s;s.p0.position=pt();s.p0.radius=number();s.p1.position=pt();s.p1.radius=number();s.control1=pt();s.control2=pt();g.segments.push_back(s);}r.geometry=std::move(g);}
 else {semantic::FilledPathMask g;g.path.fill_rule=semantic::FillRule(fill);g.path.commands.reserve(count);for(unsigned i=0;i<count;++i){const auto op=read32(b,pos);pos+=4;switch(op){case 0:g.path.commands.emplace_back(semantic::MoveTo{pt()});break;case 1:g.path.commands.emplace_back(semantic::LineTo{pt()});break;case 2:{const auto a=pt(),z=pt();g.path.commands.emplace_back(semantic::QuadTo{a,z});break;}case 3:{const auto a=pt(),c=pt(),z=pt();g.path.commands.emplace_back(semantic::CubicTo{a,c,z});break;}default:g.path.commands.emplace_back(semantic::ClosePath{});}}r.geometry=std::move(g);}
 return r;
}
std::optional<SkPath> cpuDecodePath(std::span<const std::uint8_t>b){if(b.empty()||b.size()>cpuOutputLimit||b.size()%4)return {};std::size_t read=0;auto p=SkPath::ReadFromMemory(b.data(),b.size(),&read);if(!p||read!=b.size()||!p->isFinite())return {};return p;}
}
