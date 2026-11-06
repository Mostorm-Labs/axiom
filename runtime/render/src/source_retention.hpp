#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/scene/geometry_chunk.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <map>
#include <optional>

namespace canvas::render::internal {
// Conservative retained-source storage estimates; pixel/font/driver resources
// have separate owners. A shared allocation is charged once per budget owner,
// not once per alias, frame, tile, or prepared-geometry consumer.
template<class T>std::size_t sourceContentBytes(const T& v){
    std::size_t bytes=sizeof(semantic::ObjectContent)+512;
    const auto array=[](const auto& a){return a.capacity()*sizeof(typename std::remove_cvref_t<decltype(a)>::value_type);};
    if constexpr(requires{v.stroke.knots;})bytes+=array(v.stroke.knots);
    if constexpr(requires{v.stroke.confirmed_samples;})bytes+=array(v.stroke.confirmed_samples);
    if constexpr(requires{v.stroke.vector_output.outline;})bytes+=array(v.stroke.vector_output.outline);
    if constexpr(requires{v.stroke.dab_output.dabs;})bytes+=array(v.stroke.dab_output.dabs);
    if constexpr(requires{v.stroke.snapshot.stages;}){
        bytes+=array(v.stroke.snapshot.stages)+array(v.stroke.snapshot.resources);
        for(const auto& r:v.stroke.snapshot.resources)bytes+=r.content_sha256.capacity()+1;
    }
    if constexpr(requires{v.stroke.data;})std::visit([&](const auto& d){
        if constexpr(requires{d.samples;})bytes+=array(d.samples);
        if constexpr(requires{d.dabs;})bytes+=array(d.dabs);
    },v.stroke.data);
    if constexpr(requires{v.geometry.commands;})bytes+=array(v.geometry.commands);
    if constexpr(requires{v.document.paragraphs;}){
        bytes+=array(v.document.paragraphs);for(const auto& p:v.document.paragraphs){bytes+=array(p.runs);for(const auto& r:p.runs)bytes+=r.text.capacity()+1;}
    }
    if constexpr(requires{v.rows;v.columns;v.cells;}){
        bytes+=array(v.rows)+array(v.columns)+array(v.cells);for(const auto& cell:v.cells)bytes+=sourceContentBytes(cell.text);
    }
    if constexpr(requires{v.label;})if(v.label)bytes+=sourceContentBytes(*v.label);
    if constexpr(requires{v.title.capacity();})bytes+=v.title.capacity()+1;
    if constexpr(requires{v.stroke.brush.pressure.size_curve;}){
        for(const auto* curve:{&v.stroke.brush.pressure.size_curve,&v.stroke.brush.pressure.opacity_curve})if(*curve)bytes+=array((*curve)->points);
    }
    return bytes;
}
inline std::size_t sourceContentBytes(const semantic::ObjectContent& content){return std::visit([](const auto& v){return sourceContentBytes(v);},content);}
// Per-tile unshared metadata is charged separately from shared payloads.
inline std::size_t sourceMetadataBytes(const ReferenceTraversalEntry& entry){
    const auto& r=entry.record;std::size_t bytes=r.referenceGeometryDigest.capacity()+1+
        r.properties.entries.capacity()*sizeof(semantic::PropertyEntry)+r.directDependencies.capacity()*sizeof(ObjectId)+
        (r.eraseMasks.capacity()+entry.presentationEraseMasks.capacity())*sizeof(semantic::EraseMaskRecord);
    for(const auto& p:r.properties.entries)if(const auto* style=std::get_if<semantic::StrokeStyleValue>(&p.value))
        if(const auto* stroke=std::get_if<semantic::SolidStroke>(style))if(const auto* dash=std::get_if<semantic::DashPattern>(&stroke->dash))bytes+=dash->segments.capacity()*sizeof(double);
    for(const auto masks:{std::span(r.eraseMasks),std::span(entry.presentationEraseMasks)})
    for(const auto& mask:masks)std::visit([&](const auto& g){
        if constexpr(requires{g.segments;})bytes+=g.segments.capacity()*sizeof(semantic::EraseCubicSegment);
        if constexpr(requires{g.path.commands;})bytes+=g.path.commands.capacity()*sizeof(typename std::remove_cvref_t<decltype(g.path.commands)>::value_type);
    },mask.geometry);
    if(const auto* route=std::get_if<scene::ConnectorGeometry>(&r.derivedGeometry))bytes+=
        route->route.capacity()*sizeof(foundation::WorldPoint)+route->sketchObstacles.capacity()*sizeof(WorldRect);
    return bytes;
}
struct SourceRetentionUsage final {std::size_t bytes=0;std::uint64_t admissions=0,hits=0,denied=0;};
struct SourceRetention final {
    std::shared_ptr<const void> storage;
    resources::ResourceLease lease;
    std::shared_ptr<SourceRetentionUsage> usage;
    std::size_t bytes=0;
    ~SourceRetention(){if(usage)usage->bytes-=bytes;}
};
using SourceRetentions=std::vector<std::shared_ptr<const SourceRetention>>;
class SourceRetentionPool final {
    std::map<std::weak_ptr<const void>,std::weak_ptr<const SourceRetention>,std::owner_less<std::weak_ptr<const void>>> entries_;
    std::weak_ptr<resources::ResourceBudgetManager> owner_;
    std::shared_ptr<SourceRetentionUsage> usage_=std::make_shared<SourceRetentionUsage>();
    std::shared_ptr<const SourceRetention> retain(std::shared_ptr<const void> storage,std::size_t bytes,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
        if(const auto it=entries_.find(storage);it!=entries_.end())if(auto shared=it->second.lock()){
            if(bytes>shared->bytes){++usage_->denied;return {};}
            ++usage_->hits;return shared;
        }
        auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,critical);
        if(!token){++usage_->denied;return {};}
        resources::ResourceLease lease(budget,*token);
        auto result=std::make_shared<SourceRetention>();result->storage=std::move(storage);result->lease=std::move(lease);
        result->usage=usage_;result->bytes=bytes;usage_->bytes+=bytes;++usage_->admissions;
        entries_.insert_or_assign(result->storage,result);
        if(entries_.size()>1024)std::erase_if(entries_,[](const auto& p){return p.second.expired();});
        return result;
    }
  public:
    const SourceRetentionUsage& statistics()const noexcept{return *usage_;}
    void bind(const std::shared_ptr<resources::ResourceBudgetManager>& budget){
        if(owner_.lock()!=budget){
            entries_.clear();owner_=budget;auto next=std::make_shared<SourceRetentionUsage>();
            next->admissions=usage_->admissions;next->hits=usage_->hits;next->denied=usage_->denied;usage_=std::move(next);
        }
    }
    std::optional<SourceRetentions> get(const ReferenceTraversalEntry& entry,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
        if(!budget)return {};bind(budget);SourceRetentions holds;
        const auto add=[&](std::shared_ptr<const void> storage,std::size_t bytes){
            if(!storage)return true;auto lease=retain(std::move(storage),bytes,budget,critical);if(!lease)return false;
            if(std::find(holds.begin(),holds.end(),lease)==holds.end())holds.push_back(std::move(lease));return true;
        };
        if(!add(entry.record.content.shared(),sourceContentBytes(entry.record.content.get())))return {};
        bool ok=true;std::visit([&](const auto& command){if constexpr(requires{command.content;})
            ok=add(command.content.shared(),sourceContentBytes(command.content.get()));},entry.command);
        if(!ok)return {};
        const auto& chunks=entry.record.geometryChunks;
        if(!chunks.empty()){
            std::size_t bytes=sizeof(std::vector<GeometryChunk>)+chunks.capacity()*sizeof(GeometryChunk);
            for(const auto& c:chunks)bytes+=c.digest.capacity()+1;
            if(!add(chunks.storage(),bytes))return {};
        }
        if(entry.record.geometryIndex&&!add(entry.record.geometryIndex,entry.record.geometryIndex->bytes()))return {};
        return holds;
    }
};
}
