#include "canvas/ink/compact_contour.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include <cmath>
namespace canvas::ink {
namespace {
ContourPoint mid(ContourPoint a,ContourPoint b){return {(a.x+b.x)*.5,(a.y+b.y)*.5};}
double distance2(ContourPoint a,ContourPoint b){return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y);}
}
double CompactContourCompiler::radius(const foundation::InkKnot& k,double at,double total)const {
    double r=compactRadius(config_,k.pressure);
    if(total>0){
        auto s=config_.startTaper>0?std::clamp(at/config_.startTaper,0.,1.):1.;
        auto e=config_.endTaper>0?std::clamp((total-at)/config_.endTaper,0.,1.):1.;
        r*=std::min(s*(2-s),1-std::pow(1-e,3));
    }
    return std::max(.01,r);
}
CompactContourCompiler::Anchor CompactContourCompiler::makeAnchor(const foundation::InkKnot& k,double nx,double ny,double at,double total)const {
    const auto r=radius(k,at,total);
    if(config_.family==foundation::InkFamily::kMarker){
        const auto c=std::cos(config_.markerAngle),s=std::sin(config_.markerAngle),f=config_.markerFlatness;
        const auto u=nx*c+ny*s,v=-nx*s+ny*c,den=std::hypot(u,v*f);
        if(den>1e-12){nx=(c*u-s*v*f*f)/den;ny=(s*u+c*v*f*f)/den;}
    }
    return {{k.x+nx*r,k.y+ny*r},{k.x-nx*r,k.y-ny*r}};
}
void CompactContourCompiler::accept(Anchor next,bool force,CompactContour& out){
    if(!anchor_){anchor_=midpoint_=next;anchors_=1;return;}
    const auto threshold=std::pow(config_.size*config_.smoothing,2);
    if(!force&&anchors_>1&&distance2(next.left,anchor_->left)<=threshold&&distance2(next.right,anchor_->right)<=threshold)return;
    const Anchor end{mid(anchor_->left,next.left),mid(anchor_->right,next.right)};
    lastPatch_=ContourPatch{midpoint_->left,anchor_->left,end.left,midpoint_->right,anchor_->right,end.right};
    out.patches.push_back(*lastPatch_);
    midpoint_=end;anchor_=next;++anchors_;
}
void CompactContourCompiler::append(std::span<const foundation::InkKnot> knots,double total,CompactContour& out){
    if(lastPatch_)out.patches.push_back(*lastPatch_);
    for(const auto& k:knots){
        if(current_&&(config_.startTaper>0||config_.endTaper>0)){
            const auto start=*current_;const auto length=std::hypot(k.x-start.x,k.y-start.y);
            if(length>1e-9){
                // Taper is an arc-length envelope. Sparse input must contain
                // its transition, otherwise two zero-radius endpoints erase
                // the full-width body. Only the two bounded taper zones split.
                std::vector<double> cuts;
                for(unsigned i=1;i<=8;++i){
                    const auto a=(config_.startTaper*i/8.-currentDistance_)/length;
                    const auto b=(total-config_.endTaper*i/8.-currentDistance_)/length;
                    if(config_.startTaper>0&&a>0&&a<1)cuts.push_back(a);
                    if(config_.endTaper>0&&b>0&&b<1)cuts.push_back(b);
                }
                std::sort(cuts.begin(),cuts.end());
                for(const auto t:cuts)appendKnot({std::lerp(start.x,k.x,t),std::lerp(start.y,k.y,t),
                    static_cast<std::uint16_t>(std::lround(std::lerp(double(start.pressure),double(k.pressure),t)))},total,out);
            }
        }
        appendKnot(k,total,out);
    }
}
void CompactContourCompiler::appendKnot(const foundation::InkKnot& k,double total,CompactContour& out){
        if(current_&&*current_==k)return;
        ++visits_;
        if(!current_){current_=k;return;}
        auto dx=k.x-current_->x,dy=k.y-current_->y,l=std::hypot(dx,dy);
        if(l<1e-9){current_=k;return;}
        const auto nx=-dy/l,ny=dx/l;
        if(!previous_){
            accept(makeAnchor(*current_,nx,ny,0,total),true,out);
            if(config_.startCap&&config_.startTaper==0)out.caps.push_back({current_->x,current_->y,radius(*current_,0,total)});
        }else{
            const auto px=current_->x-previous_->x,py=current_->y-previous_->y,pl=std::hypot(px,py);
            const auto ox=pl>0?-py/pl:nx,oy=pl>0?px/pl:ny,dot=ox*nx+oy*ny;
            if(dot<0){
                // The round join lives at this actual knot. Midpoint smoothing
                // must reach its incoming support before changing direction;
                // otherwise the separate disk can be an isolated component.
                const auto incoming=makeAnchor(*current_,ox,oy,currentDistance_,total);
                const auto outgoing=makeAnchor(*current_,nx,ny,currentDistance_,total);
                lastPatch_=ContourPatch{midpoint_->left,anchor_->left,incoming.left,
                                       midpoint_->right,anchor_->right,incoming.right};
                out.patches.push_back(*lastPatch_);
                // Both support pairs and the common control are in the same
                // round/marker-ellipse node. This bridge cannot add a miter or
                // a bulge outside the node, even at a degenerate 180-degree U.
                const ContourPoint center{current_->x,current_->y};
                lastPatch_=ContourPatch{incoming.left,center,outgoing.left,
                                       incoming.right,center,outgoing.right};
                out.patches.push_back(*lastPatch_);
                anchor_=midpoint_=outgoing;++anchors_;
                out.caps.push_back({current_->x,current_->y,radius(*current_,currentDistance_,total)});
            }else{
                const auto ax=nx+(ox-nx)*dot,ay=ny+(oy-ny)*dot;
                accept(makeAnchor(*current_,ax,ay,currentDistance_,total),false,out);
            }
        }
        previous_=current_;current_=k;distance_+=l;currentDistance_=distance_;

}

void CompactContourCompiler::finish(double total,CompactContour& out){
    if(!current_)return;
    if(!previous_){out.caps.push_back({current_->x,current_->y,compactRadius(config_,current_->pressure)});return;}
    const auto dx=current_->x-previous_->x,dy=current_->y-previous_->y,l=std::hypot(dx,dy);
    accept(makeAnchor(*current_,l>0?-dy/l:0,l>0?dx/l:1,currentDistance_,total),true,out);
    if(anchor_&&midpoint_)out.patches.push_back({midpoint_->left,anchor_->left,anchor_->left,midpoint_->right,anchor_->right,anchor_->right});
    if(config_.endCap&&config_.endTaper==0)out.caps.push_back({current_->x,current_->y,radius(*current_,currentDistance_,total)});
}
CompactContour compileCompactContour(const foundation::InkBrushConfig& config,std::span<const foundation::InkKnot> knots){
    CompactContour result;CompactContourCompiler compiler(config);const auto total=compactLength(knots);
    compiler.append(knots,total,result);compiler.finish(total,result);return result;
}
}
