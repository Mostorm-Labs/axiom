#pragma once
#include "canvas/foundation/world_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>
namespace canvas::render {
struct TileKey final {std::int64_t x=0,y=0;std::int32_t scale=0;auto operator<=>(const TileKey&)const=default;};
struct TileRasterRequest final {TileKey key;std::uint64_t generation=0;};
// Candidate configuration is private engineering policy, not a public ladder.
class TileGrid final {
 public:
 explicit TileGrid(double worldSize=256,std::size_t capacity=4096):size_(worldSize),capacity_(capacity){}
 void setEnabled(bool enabled)noexcept{enabled_=enabled;}
 std::vector<TileKey> coverage(const foundation::WorldRect& rect,std::int32_t scale,std::size_t limit)const{
  std::vector<TileKey> out;
  if(!rect.isFiniteAndOrdered()||rect.right<=rect.left||rect.bottom<=rect.top||!std::isfinite(size_)||size_<=0)return out;
  if(scale<-30||scale>30)return out;
  const double unit=std::ldexp(size_,-scale);
  if(!std::isfinite(unit)||unit<=0)return out;
  const auto x0=std::floor(rect.left/unit),x1=std::ceil(rect.right/unit)-1,y0=std::floor(rect.top/unit),y1=std::ceil(rect.bottom/unit)-1;
  if(std::max({std::abs(x0),std::abs(x1),std::abs(y0),std::abs(y1)})>double(INT64_MAX/2))return out;
  if((x1-x0+1)*(y1-y0+1)>double(limit))return out;
  for(auto y=std::int64_t(y0);y<=std::int64_t(y1);++y)for(auto x=std::int64_t(x0);x<=std::int64_t(x1);++x)out.push_back({x,y,scale});
  return out;
 }
 foundation::WorldRect bounds(TileKey key)const{const double unit=std::ldexp(size_,-key.scale);return {float(key.x*unit),float(key.y*unit),float((key.x+1)*unit),float((key.y+1)*unit)};}
 void markDirty(const foundation::WorldRect& rect){for(auto& [key,row]:rows_)if(intersects(bounds(key),rect)){if(nextGeneration_==UINT64_MAX){rows_.clear();return;}row.generation=++nextGeneration_;row.clean=false;}}
 void invalidate(){for(auto& [key,row]:rows_){(void)key;if(nextGeneration_==UINT64_MAX){rows_.clear();return;}row.generation=++nextGeneration_;row.clean=false;}}
 std::vector<TileRasterRequest> requests(const foundation::WorldRect& visible,std::int32_t scale,std::size_t max){
  std::vector<TileRasterRequest> out;if(!enabled_)return out;
  for(const auto key:coverage(visible,scale,capacity_)){
   if(!rows_.contains(key)){
    if(rows_.size()>=capacity_||nextGeneration_==UINT64_MAX)return {};
    rows_.emplace(key,Row{++nextGeneration_,false});
   }
   auto& row=rows_.at(key);
   if(!row.clean&&out.size()<max)out.push_back({key,row.generation});
  }return out;
 }
 bool complete(const TileRasterRequest& request){auto it=rows_.find(request.key);if(it==rows_.end()||it->second.generation!=request.generation)return false;it->second.clean=true;return true;}
 void evict(TileKey key){rows_.erase(key);}
 private:
 static bool intersects(const foundation::WorldRect& a,const foundation::WorldRect& b){return a.left<b.right&&a.right>b.left&&a.top<b.bottom&&a.bottom>b.top;}
 struct Row{std::uint64_t generation=1;bool clean=false;};double size_;std::size_t capacity_;bool enabled_=false;std::uint64_t nextGeneration_=0;std::map<TileKey,Row> rows_;
};
}
