#include "canvas/render/damage_tracker.hpp"
#include "canvas/render/render_group_manager.hpp"
#include "canvas/render/render_cache.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void damage(){
    render::DamageTracker tracker;
    const render::VisualContribution old{{0,0,10,10},true,false},moved{{100,0,110,10},true,false};
    const std::vector<render::VisualContributionDelta> changes{{ObjectId::fromUint64(1),old,moved}};
    const auto result=tracker.collect(SceneRevision{1},SceneRevision{2},changes);
    require(!result.fullRedraw&&result.rects.size()==2&&result.exactUnionArea==200&&result.damageArea==200,"ordinary move promotes global damage or fills empty world");
    const auto collapsed=render::DamageTracker{1}.collect(SceneRevision{1},SceneRevision{2},changes);
    require(collapsed.rects.size()==1&&collapsed.exactUnionArea==200&&collapsed.damageArea==1100,"bounded merge shrank damage");
    for(const auto& delta:std::vector<render::VisualContributionDelta>{
        {ObjectId::fromUint64(1),std::nullopt,old},{ObjectId::fromUint64(1),old,std::nullopt},
        {ObjectId::fromUint64(1),old,render::VisualContribution{{0,0,10,10},false,false}}}){
        const auto d=tracker.collect(SceneRevision{1},SceneRevision{2},{&delta,1});
        require(d.damageArea==100&&!d.fullRedraw,"create/delete/hide loses old or new extent");
    }
    auto dependent=changes;dependent.front().after->requiresBackdrop=true;
    require(tracker.collect(SceneRevision{1},SceneRevision{2},dependent).fullRedraw,"unbounded backdrop accepted as local");
    std::vector<render::VisualContributionDelta> offscreen;
    for(unsigned i=0;i<10000;++i)offscreen.push_back({ObjectId::fromUint64(i+1),std::nullopt,render::VisualContribution{{10000.F+i*10,0,10005.F+i*10,10},true,false}});
    const auto d=tracker.collect(SceneRevision{1},SceneRevision{2},offscreen);
    require(d.rects.size()<=64&&!d.fullRedraw&&d.inputRectCount==10000&&d.exactUnionArea==-1,"offscreen batch lost bounded metadata");
    SceneRecord before;before.objectId=ObjectId::fromUint64(1);before.flags=SceneRecordFlags::kVisible;before.worldBounds={0,0,10,10};
    auto after=before;after.worldBounds={20,0,30,10};
    SceneDelta scene;scene.generationFrom=SceneRevision{1};scene.generationTo=SceneRevision{2};
    scene.mutations={{SceneMutationKind::kUpdate,before.objectId,before,after}};
    const auto fromScene=tracker.collect(scene);
    require(fromScene.before==SceneRevision{1}&&fromScene.after==SceneRevision{2}&&fromScene.exactUnionArea==200,"SceneDelta bridge changed generation or visual contributions");
}
render::ReferenceTraversalEntry source(){
    render::ReferenceTraversalEntry value;value.record.objectId=ObjectId::fromUint64(1);
    value.record.kindVersion=1;value.record.worldBounds={0,0,20,20};value.record.content=semantic::ShapeContent{1,20,20};
    value.command=render::ShapeReferenceCommand{std::get<semantic::ShapeContent>(value.record.content.get())};return value;
}
void revisions(){
    render::RenderGroupManager manager;auto value=source();const WorldRect clip{0,0,100,100};
    require(manager.update({&value,1},clip,1).contributions.size()==1,"new source not damaged");
    const auto initial=manager.find(value.record.objectId)->revisions;
    require(manager.update({&value,1},clip,1).contributions.empty(),"unchanged source dirty");
    value.record.transform.tx=10;value.record.worldBounds={10,0,30,20};
    (void)manager.update({&value,1},clip,1);auto rev=manager.find(value.record.objectId)->revisions;
    require(rev.content==initial.content&&rev.transform==initial.transform+1,"transform rebuilt content identity");
    const auto afterTransform=rev;(void)manager.update({&value,1},{10,0,90,100},1);rev=manager.find(value.record.objectId)->revisions;
    require(rev.clip==afterTransform.clip+1&&rev.content==afterTransform.content,"clip conflated with content");
    (void)manager.update({&value,1},{10,0,90,100},2);rev=manager.find(value.record.objectId)->revisions;
    require(rev.resource==initial.resource+1&&rev.content==initial.content,"resource conflated with content");
    value.geometryChunked=true;value.geometryChunkIndices={1};
    (void)manager.update({&value,1},clip,2);rev=manager.find(value.record.objectId)->revisions;
    require(rev.geometry==initial.geometry+1&&rev.content==initial.content,
            "geometry selection conflated with content");
    value.geometryChunkIndices={2};
    const auto changedGeometry=manager.update({&value,1},clip,2);
    rev=manager.find(value.record.objectId)->revisions;
    require(rev.geometry==initial.geometry+2&&rev.content==initial.content&&
            !changedGeometry.contributions.empty(),"geometry-only selection reused stale raster");
    require(manager.update({&value,1},clip,2).contributions.empty(),
            "unchanged geometry selection repeatedly invalidated raster");
    value.record.properties.entries.push_back({3U,.5F});(void)manager.update({&value,1},clip,2);rev=manager.find(value.record.objectId)->revisions;
    require(rev.effect==initial.effect+1&&rev.content==initial.content,"effect revision not independent");
    auto other=value;other.record.objectId=ObjectId::fromUint64(2);
    std::vector<render::ReferenceTraversalEntry> list{value,other};(void)manager.update(list,clip,2);
    const auto beforeOrder=manager.find(value.record.objectId)->revisions;
    std::reverse(list.begin(),list.end());(void)manager.update(list,clip,2);rev=manager.find(value.record.objectId)->revisions;
    require(rev.order==beforeOrder.order+1&&rev.content==beforeOrder.content,"order invalidated content");
    render::RenderCacheValidity cached{beforeOrder,1,true},current{rev,1,true};
    current.revisions.transform+=1;
    require(render::RenderSourceResolver::resolve(cached,current)==render::RenderSourceKind::kRasterCache,"eligible transform-only source cannot be reused");
    current.revisions.resource+=1;
    require(render::RenderSourceResolver::resolve(cached,current)==render::RenderSourceKind::kDirect,"stale resource source reused");
    // RenderGroup cardinality does not require constructing a child-sized
    // raster or touching child content on an aggregate transform revision.
    std::vector<render::RenderGroup> children(100000);
    render::RenderGroup huge{{ObjectId::fromUint64(9),1},{}};
    const auto base=huge.revisions;huge.apply({.transform=true});
    for(const auto& child:children)require(child.revisions.content==1&&child.revisions.transform==1,"aggregate transform mutated child revisions");
    require(huge.revisions.content==base.content&&huge.revisions.transform==base.transform+1,"group transform rebuilt children");
}
}
int main(){try{damage();revisions();std::cout<<"Render-owned old/new damage and dependency revisions agree\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
