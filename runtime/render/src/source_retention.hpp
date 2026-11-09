#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/scene/geometry_chunk.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <map>
#include <optional>
#include <functional>
#include <limits>

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
inline std::size_t sourceMaskBytes(const std::vector<semantic::EraseMaskRecord>& masks){
    std::size_t bytes=sizeof(masks)+masks.capacity()*sizeof(semantic::EraseMaskRecord);
    for(const auto& mask:masks)std::visit([&](const auto& g){
        if constexpr(requires{g.segments;})bytes+=g.segments.capacity()*sizeof(semantic::EraseCubicSegment);
        else bytes+=g.path.commands.capacity()*sizeof(semantic::PathCommand);
    },mask.geometry);
    return bytes;
}
// Per-tile unshared metadata is charged separately from shared payloads.
inline std::size_t sourceMetadataBytes(const ReferenceTraversalEntry& entry){
    const auto& r=entry.record;std::size_t bytes=r.referenceGeometryDigest.capacity()+1+
        r.properties.entries.capacity()*sizeof(semantic::PropertyEntry)+r.directDependencies.capacity()*sizeof(ObjectId)+
        entry.presentationEraseMasks.capacity()*sizeof(semantic::EraseMaskRecord);
    for(const auto& p:r.properties.entries)if(const auto* style=std::get_if<semantic::StrokeStyleValue>(&p.value))
        if(const auto* stroke=std::get_if<semantic::SolidStroke>(style))if(const auto* dash=std::get_if<semantic::DashPattern>(&stroke->dash))bytes+=dash->segments.capacity()*sizeof(double);
    for(const auto masks:{std::span(entry.presentationEraseMasks)})
    for(const auto& mask:masks)std::visit([&](const auto& g){
        if constexpr(requires{g.segments;})bytes+=g.segments.capacity()*sizeof(semantic::EraseCubicSegment);
        if constexpr(requires{g.path.commands;})bytes+=g.path.commands.capacity()*sizeof(typename std::remove_cvref_t<decltype(g.path.commands)>::value_type);
    },mask.geometry);
    if(const auto* route=std::get_if<scene::ConnectorGeometry>(&r.derivedGeometry))bytes+=
        route->route.capacity()*sizeof(foundation::WorldPoint)+route->sketchObstacles.capacity()*sizeof(WorldRect);
    return bytes;
}
// A shared control block alone does not identify an aliased subobject/type.
// Strong keys are used only by the bounded prepared alias index. The source
// pool indexes weak ownership; each live retention pins its allocation.
template<class T>const void* sourceTypeTag(){static char tag;return &tag;}
struct ImmutableSourceIdentity final {
    std::shared_ptr<const void> storage;
    const void* type=nullptr;
    std::size_t command=0;
};
struct ImmutableSourceIdentityLess final {
    bool operator()(const ImmutableSourceIdentity& a,const ImmutableSourceIdentity& b)const noexcept{
        if(a.storage.owner_before(b.storage))return true;
        if(b.storage.owner_before(a.storage))return false;
        if(a.storage.get()!=b.storage.get())return std::less<const void*>{}(a.storage.get(),b.storage.get());
        if(a.type!=b.type)return std::less<const void*>{}(a.type,b.type);
        return a.command<b.command;
    }
};
struct SourceRetentionUsage final {
    std::size_t bytes=0,indexBytes=0;
    std::uint64_t admissions=0,hits=0,denied=0,quotaDenials=0,footprintEvaluations=0,knownFootprintHits=0,footprintChunks=0;
};
struct SourceRetention final {
    struct View {
        const void* pointee=nullptr;const void* type=nullptr;
        bool operator<(const View& b)const noexcept{
            if(pointee!=b.pointee)return std::less<const void*>{}(pointee,b.pointee);
            return std::less<const void*>{}(type,b.type);
        }
    };
    std::shared_ptr<const void> storage;
    resources::ResourceLease lease;
    std::shared_ptr<SourceRetentionUsage> usage;
    std::size_t bytes=0,indexBytes=0;
    // Each extra known view owns its metadata reservation. Unknown aliases
    // always evaluate the existing conservative footprint before admission.
    std::map<View,resources::ResourceLease> views;
    std::function<void()> retireIndex;
    ~SourceRetention(){if(retireIndex)retireIndex();if(usage){usage->bytes-=bytes;usage->indexBytes-=indexBytes;}}
};
using SourceRetentions=std::vector<std::shared_ptr<const SourceRetention>>;
class SourceRetentionPool final {
    using Owners=std::map<std::weak_ptr<const void>,std::weak_ptr<SourceRetention>,std::owner_less<std::weak_ptr<const void>>>;
    struct Index {Owners entries;};
    std::shared_ptr<Index> index_=std::make_shared<Index>();
    std::weak_ptr<resources::ResourceBudgetManager> owner_;
    std::shared_ptr<SourceRetentionUsage> usage_=std::make_shared<SourceRetentionUsage>();
    static constexpr std::size_t viewCharge=sizeof(SourceRetention::View)+sizeof(resources::ResourceLease)+128;
    static constexpr std::size_t ownerCharge=sizeof(SourceRetention)+sizeof(Owners::value_type)+256;
    template<class F>std::shared_ptr<const SourceRetention> retain(std::shared_ptr<const void> storage,const void* type,F&& footprint,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
        const SourceRetention::View view{storage.get(),type};
        if(const auto it=index_->entries.find(storage);it!=index_->entries.end())if(auto shared=it->second.lock()){
            if(shared->views.contains(view)){++usage_->hits;++usage_->knownFootprintHits;return shared;}
            ++usage_->footprintEvaluations;const auto bytes=footprint();
            // Preserve full validation and the existing allocation bound for
            // an unproven aliased view; ownership alone never skips this work.
            if(bytes>shared->bytes){++usage_->denied;return {};}
            auto token=budget->reserve(resources::ResourceClass::kRecorded,viewCharge,critical);
            if(!token){++usage_->denied;++usage_->quotaDenials;return {};}
            resources::ResourceLease admission(budget,*token);
            shared->views.emplace(view,std::move(admission));shared->indexBytes+=viewCharge;usage_->indexBytes+=viewCharge;
            ++usage_->hits;return shared;
        }
        // Every owner/view node is reserved before insertion, so the existing
        // quota bounds cardinality. Last-release callbacks remove expired
        // nodes immediately; no new source-count cap or all-owner scan.

        ++usage_->footprintEvaluations;const auto bytes=footprint();
        if(bytes>std::numeric_limits<std::size_t>::max()-ownerCharge-viewCharge){++usage_->denied;return {};}
        const auto metadata=ownerCharge+viewCharge;
        auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes+metadata,critical);
        if(!token){++usage_->denied;++usage_->quotaDenials;return {};}
        resources::ResourceLease lease(budget,*token);
        auto result=std::make_shared<SourceRetention>();result->storage=std::move(storage);result->lease=std::move(lease);
        result->views.emplace(view,resources::ResourceLease{});
        result->usage=usage_;result->bytes=bytes;result->indexBytes=metadata;usage_->bytes+=bytes;usage_->indexBytes+=metadata;
        const auto weakIndex=std::weak_ptr<Index>(index_);const auto key=std::weak_ptr<const void>(result->storage);
        result->retireIndex=[weakIndex,key]{if(auto index=weakIndex.lock()){
            const auto it=index->entries.find(key);if(it!=index->entries.end()&&it->second.expired())index->entries.erase(it);
        }};
        index_->entries.insert_or_assign(result->storage,result);++usage_->admissions;return result;
    }
  public:
    const SourceRetentionUsage& statistics()const noexcept{return *usage_;}
    std::shared_ptr<const SourceRetention> getMasks(const scene::SharedVector<semantic::EraseMaskRecord>& masks,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
        if(!budget||masks.empty())return {};bind(budget);
        return retain(masks.storage(),sourceTypeTag<std::vector<semantic::EraseMaskRecord>>(),
            [&]{return sourceMaskBytes(masks);},budget,critical);
    }
    void bind(const std::shared_ptr<resources::ResourceBudgetManager>& budget){
        if(owner_.lock()!=budget){
            index_=std::make_shared<Index>();owner_=budget;auto next=std::make_shared<SourceRetentionUsage>(*usage_);
            next->bytes=next->indexBytes=0;usage_=std::move(next);
        }
    }
    std::optional<SourceRetentions> get(const ReferenceTraversalEntry& entry,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
        if(!budget)return {};bind(budget);SourceRetentions holds;
        const auto add=[&](auto storage,auto&& footprint){
            if(!storage)return true;
            using Value=std::remove_const_t<typename decltype(storage)::element_type>;
            auto lease=retain(storage,sourceTypeTag<Value>(),std::forward<decltype(footprint)>(footprint),budget,critical);
            if(!lease)return false;
            if(std::find(holds.begin(),holds.end(),lease)==holds.end())holds.push_back(std::move(lease));return true;
        };
        if(!add(entry.record.content.shared(),[&]{return sourceContentBytes(entry.record.content.get());}))return {};
        bool ok=true;std::visit([&](const auto& command){if constexpr(requires{command.content;})
            ok=add(command.content.shared(),[&]{return sourceContentBytes(command.content.get());});},entry.command);
        if(!ok)return {};
        if(!entry.record.eraseMasks.empty()){
            auto masks=getMasks(entry.record.eraseMasks,budget,critical);if(!masks)return {};
            holds.push_back(std::move(masks));
        }
        const auto& chunks=entry.record.geometryChunks;
        if(!chunks.empty()&&!add(chunks.storage(),[&]{
            std::size_t bytes=sizeof(std::vector<GeometryChunk>)+chunks.capacity()*sizeof(GeometryChunk);
            for(const auto& c:chunks){++usage_->footprintChunks;bytes+=c.digest.capacity()+1;}return bytes;
        }))return {};
        if(entry.record.geometryIndex&&!add(entry.record.geometryIndex,[&]{return entry.record.geometryIndex->bytes();}))return {};
        return holds;
    }
};
}
