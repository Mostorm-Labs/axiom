#include "canvas/ink/vector_stroke_reference.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace canvas::ink::reference {
namespace {
constexpr double kMinStreamlineT = 0.15;
constexpr double kStreamlineRange = 0.85;
constexpr double kDefaultFirstPressure = 0.25;
constexpr double kDefaultPressure = 0.5;
constexpr double kMinRadius = 0.01;
constexpr double kFixedPi = std::numbers::pi + 0.0001;
constexpr std::size_t kStartCapSegments = 13;
constexpr std::size_t kEndCapSegments = 29;
constexpr std::size_t kCornerCapSegments = 13;
constexpr double kEndNoiseThreshold = 3.0;
struct P { double x, y; };
P add(P a,P b){return {a.x+b.x,a.y+b.y};} P sub(P a,P b){return {a.x-b.x,a.y-b.y};}
P mul(P a,double n){return {a.x*n,a.y*n};} double dot(P a,P b){return a.x*b.x+a.y*b.y;}
double len(P a){return std::hypot(a.x,a.y);} double dist(P a,P b){return len(sub(a,b));} double dist2(P a,P b){P d=sub(a,b);return dot(d,d);}
P uni(P a){double n=len(a); return n==0?P{0,0}:mul(a,1.0/n);} P per(P a){return {a.y,-a.x};}
P rot(P a,P c,double r){double s=std::sin(r),co=std::cos(r),x=a.x-c.x,y=a.y-c.y;return {x*co-y*s+c.x,x*s+y*co+c.y};}
P lrp(P a,P b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};}
double radius(double size,double thinning,double pressure){return size*(0.5-thinning*(0.5-pressure));}
double simulate(double prev,double d,double size){double sp=std::min(1.0,d/size),rp=std::min(1.0,1.0-sp);return std::min(1.0,prev+(rp-prev)*(sp*0.275));}
double taperDistance(bool enabled,bool fullLength,double value,double size,double total){
  if(!enabled) return 0.0;
  return fullLength ? std::max(size,total) : value;
}
double startEase(double t){return t*(2.0-t);}
double endEase(double t){--t; return t*t*t+1.0;}
std::vector<StrokeOutlinePoint> drawDot(P c,double r){
  P offset=add(c,{1,1}); P start=add(c,mul(uni(per(sub(c,offset))),-r));
  std::vector<StrokeOutlinePoint> out; for(std::size_t i=1;i<=kStartCapSegments;i++){P q=rot(start,c,kFixedPi*2.0*static_cast<double>(i)/kStartCapSegments);out.push_back({q.x,q.y});} return out;
}
std::vector<StrokeOutlinePoint> drawRoundStart(P c,P right){std::vector<StrokeOutlinePoint> out;for(std::size_t i=1;i<=kStartCapSegments;i++){P q=rot(right,c,kFixedPi*static_cast<double>(i)/kStartCapSegments);out.push_back({q.x,q.y});}return out;}
std::vector<StrokeOutlinePoint> drawFlatStart(P c,P left,P right){P cv=sub(left,right),a=mul(cv,.5),b=mul(cv,.51);return {{c.x-a.x,c.y-a.y},{c.x-b.x,c.y-b.y},{c.x+b.x,c.y+b.y},{c.x+a.x,c.y+a.y}};}
std::vector<StrokeOutlinePoint> drawRoundEnd(P c,P direction,double r){std::vector<StrokeOutlinePoint> out;P start=add(c,mul(uni(direction),r));for(std::size_t i=1;i<=kEndCapSegments;i++){P q=rot(start,c,kFixedPi*3.0*static_cast<double>(i)/kEndCapSegments);out.push_back({q.x,q.y});}return out;}
std::vector<StrokeOutlinePoint> drawFlatEnd(P c,P direction,double r){P d=mul(uni(direction),r);return {{c.x+d.x,c.y+d.y},{c.x+d.x*.99,c.y+d.y*.99},{c.x-d.x*.99,c.y-d.y*.99},{c.x-d.x,c.y-d.y}};}
}
std::vector<StrokePoint> getStrokePoints(std::span<const VectorStrokeInput> input,const StrokeOptions& o){
 if(input.empty()) return {}; const double t=kMinStreamlineT+(1-o.streamline)*kStreamlineRange; std::vector<VectorStrokeInput> pts(input.begin(),input.end());
 if(pts.size()==2){auto last=pts[1];pts.resize(1);for(int i=1;i<5;i++)pts.push_back({pts[0].x+(last.x-pts[0].x)*i/4.0,pts[0].y+(last.y-pts[0].y)*i/4.0,std::numeric_limits<double>::quiet_NaN()});}
 if(pts.size()==1) pts.push_back({pts[0].x+1,pts[0].y+1,pts[0].pressure});
 std::vector<StrokePoint> out{{pts[0].x,pts[0].y,std::isfinite(pts[0].pressure)&&pts[0].pressure>=0?pts[0].pressure:kDefaultFirstPressure,1,1,0,0}}; bool reached=false; double running=0;
 for(std::size_t i=1;i<pts.size();i++){P prev{out.back().x,out.back().y};P raw{pts[i].x,pts[i].y};P point=(o.last&&i==pts.size()-1)?raw:lrp(prev,raw,t);if(point.x==prev.x&&point.y==prev.y)continue;double d=dist(point,prev);running+=d;if(i<pts.size()-1&&!reached){if(running<o.size)continue;reached=true;}P v=uni(sub(prev,point));out.push_back({point.x,point.y,std::isfinite(pts[i].pressure)&&pts[i].pressure>=0?pts[i].pressure:kDefaultPressure,v.x,v.y,d,running});}
 out[0].vector_x=out.size()>1?out[1].vector_x:0;out[0].vector_y=out.size()>1?out[1].vector_y:0; return out;
}
std::vector<StrokeOutlinePoint> getStrokeOutlinePoints(std::span<const StrokePoint> points,const StrokeOptions& o){
 if(points.empty()||o.size<=0)return {};
 const double total=points.back().running_length;
 const double ts=taperDistance(o.start_taper_enabled,o.start_taper_full_length,o.start_taper,o.size,total);
 const double te=taperDistance(o.end_taper_enabled,o.end_taper_full_length,o.end_taper,o.size,total);
 const double minD=std::pow(o.size*o.smoothing,2);
 std::vector<StrokeOutlinePoint> left,right;
 double prevPressure=points[0].pressure;
 const std::size_t initial=std::min<std::size_t>(10,points.size());
 for(std::size_t j=0;j<initial;j++){double p=points[j].pressure;if(o.simulate_pressure)p=simulate(prevPressure,points[j].distance,o.size);prevPressure=(prevPressure+p)/2.0;}
 double r=radius(o.size,o.thinning,points.back().pressure),firstR=0;
 P prevV{points[0].vector_x,points[0].vector_y};
 P prevL{points[0].x,points[0].y},prevR=prevL,tempL=prevL,tempR=prevR;
 bool first=true,prevSharp=false;
 for(std::size_t i=0;i<points.size();i++){
   const auto& q=points[i]; const bool last=i+1==points.size();
   if(!last&&total-q.running_length<kEndNoiseThreshold)continue;
   double pressure=q.pressure;
   if(o.thinning){if(o.simulate_pressure)pressure=simulate(prevPressure,q.distance,o.size);r=radius(o.size,o.thinning,pressure);}else r=o.size/2.0;
   if(first){firstR=r;first=false;}
   const double ss=ts>0&&q.running_length<ts?startEase(q.running_length/ts):1.0;
   const double es=te>0&&total-q.running_length<te?endEase((total-q.running_length)/te):1.0;
   r=std::max(kMinRadius,r*std::min(ss,es));
   P point{q.x,q.y},v{q.vector_x,q.vector_y}; P next=last?v:P{points[i+1].vector_x,points[i+1].vector_y};
   const double nd=last?1.0:dot(v,next),pd=dot(v,prevV);
   const bool sharpPoint=pd<0&&!prevSharp, sharpNext=nd<0;
   if(sharpPoint||sharpNext){P off=mul(per(prevV),r);for(std::size_t j=0;j<=kCornerCapSegments;j++){double t=static_cast<double>(j)/kCornerCapSegments;tempL=rot(sub(point,off),point,kFixedPi*t);tempR=rot(add(point,off),point,-kFixedPi*t);left.push_back({tempL.x,tempL.y});right.push_back({tempR.x,tempR.y});}prevL=tempL;prevR=tempR;if(sharpNext)prevSharp=true;continue;}
   prevSharp=false;
   if(last){P off=mul(per(v),r);tempL=sub(point,off);tempR=add(point,off);left.push_back({tempL.x,tempL.y});right.push_back({tempR.x,tempR.y});continue;}
   P off=mul(per(lrp(next,v,nd)),r);tempL=sub(point,off);tempR=add(point,off);
   if(i<=1||dist2(tempL,prevL)>minD){left.push_back({tempL.x,tempL.y});prevL=tempL;}
   if(i<=1||dist2(tempR,prevR)>minD){right.push_back({tempR.x,tempR.y});prevR=tempR;}
   prevPressure=pressure;prevV=v;
 }
 const P firstP{points[0].x,points[0].y}; const P lastP=points.size()>1?P{points.back().x,points.back().y}:add(firstP,{1,1});
 if(points.size()==1&&((!ts&&!te)||o.last))return drawDot(firstP,firstR?firstR:r);
 if(points.size()==1){std::reverse(right.begin(),right.end());left.insert(left.end(),right.begin(),right.end());return left;}
 std::vector<StrokeOutlinePoint> startCap,endCap;
 if(ts>0){} else if(o.start_cap&&!right.empty())startCap=drawRoundStart(firstP,P{right.front().x,right.front().y}); else if(!left.empty()&&!right.empty())startCap=drawFlatStart(firstP,P{left.front().x,left.front().y},P{right.front().x,right.front().y});
 P direction=per(P{-points.back().vector_x,-points.back().vector_y});
 if(te>0 || (ts>0 && points.size()==1))endCap.push_back({lastP.x,lastP.y}); else if(o.end_cap)endCap=drawRoundEnd(lastP,direction,r); else endCap=drawFlatEnd(lastP,direction,r);
 std::reverse(right.begin(),right.end());
 left.insert(left.end(),endCap.begin(),endCap.end());left.insert(left.end(),right.begin(),right.end());left.insert(left.end(),startCap.begin(),startCap.end());return left;
}
} // namespace canvas::ink::reference
