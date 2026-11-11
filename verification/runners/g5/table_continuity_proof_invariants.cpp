// Canonical-only verification of the captured mixed sequence and Cancel/History.
#include "canvas/semantic/snapshot.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace canvas;
using nlohmann::json;
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
semantic::SemanticSnapshot read(const std::string& p){std::ifstream f(p,std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(d.ok(),"canonical snapshot decode");return std::move(*d.snapshot);}
const semantic::ObjectRecord& find(const semantic::SemanticSnapshot& s,foundation::ObjectId id){const auto f=std::find_if(s.objects.begin(),s.objects.end(),[&](const auto&o){return o.id==id;});need(f!=s.objects.end(),"object disappeared");return *f;}
semantic::ObjectRecord moved(semantic::ObjectRecord o,double x,double y){o.transform.tx+=x;o.transform.ty+=y;return o;}
int main(int argc,char**argv){try{
 need(argc==2,"usage row_directory");const std::string dir=argv[1];
 const auto initial=read(dir+"/initial.axsnap"),final=read(dir+"/final.axsnap"),undo=read(dir+"/undo.axsnap"),redo=read(dir+"/redo.axsnap");
 need(initial.objects.size()==44&&final.objects.size()==44,"fixed44 cardinality");
 need(final.objects==redo.objects&&final.document_digest==redo.document_digest,"Redo canonical equality");
 need(initial.document_id==final.document_id&&initial.document_id==undo.document_id,"document identity");
 need(initial.manifest==final.manifest&&final.manifest==undo.manifest&&undo.manifest==redo.manifest,"resource manifest changed");
 need(initial.background==final.background&&final.background==undo.background,"background changed");
 std::size_t bodies=0,oldMasks=0,newMasks=0,sticky=0,table=0,image=0,other=0;
 for(const auto&before:initial.objects){const auto&after=find(final,before.id);const auto&u=find(undo,before.id);
  if(before.kind==semantic::ObjectKind::kVectorStroke){
   ++bodies;auto a=before,b=after;a.erase_masks.clear();b.erase_masks.clear();need(a==b,"unchanged stroke body/content/placement/material changed");need(after==u,"Undo image unexpectedly changed stroke");
   oldMasks+=before.erase_masks.size();newMasks+=after.erase_masks.size();need(after.erase_masks.size()>=before.erase_masks.size(),"historical masks removed");
   for(const auto&m:before.erase_masks)need(std::find(after.erase_masks.begin(),after.erase_masks.end(),m)!=after.erase_masks.end(),"historical mask mutated");
  }else if(before.kind==semantic::ObjectKind::kSticky){++sticky;need(moved(before,80,30)==after&&after==u,"sticky legitimate move/History mismatch");}
  else if(before.kind==semantic::ObjectKind::kTable){++table;need(before==after&&after==u,"table Cancel mutated canonical content");}
  else if(before.kind==semantic::ObjectKind::kImage){++image;need(moved(before,80,30)==after&&before==u,"image transform/Undo mismatch");}
  else{++other;need(before==after&&after==u,"unrelated object mutated");}
 }
 need(bodies==40&&sticky==1&&table==1&&image==1&&other==1,"mixed kind identity");
 json result{{"status","QUALIFIED"},{"unchanged_long_bodies",bodies},{"historical_masks_preserved",oldMasks},{"final_masks",newMasks},{"table_cancel_no_mutation",true},{"sticky_only_80_30",true},{"image_undo_pose_only",true},{"resource_manifest_unchanged",true},{"redo_canonical_equal",true}};
 std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
