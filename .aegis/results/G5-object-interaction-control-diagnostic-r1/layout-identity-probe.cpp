#include "canvas/render/render_group_manager.hpp"
#include "canvas/text/text_layout.hpp"
#include <iostream>
using namespace canvas;using namespace canvas::render;
int main(){
 for(unsigned kind=0;kind<3;++kind){
  RenderGroupManager manager;ReferenceTraversalEntry first;
  first.record.objectId=foundation::ObjectId::fromUint64(77+kind);
  auto a=std::make_shared<text::TextLayoutSnapshot>();a->digest="same-layout";a->localBounds={0,0,120,80};
  if(kind==0){first.record.kind=semantic::ObjectKind::kShape;first.record.kindVersion=1;first.record.content=semantic::ShapeContent{1,120,80};first.command=ShapeReferenceCommand{semantic::ShapeContent{1,120,80},a,true};}
  if(kind==1){first.record.kind=semantic::ObjectKind::kSticky;first.record.kindVersion=1;first.record.content=semantic::StickyContent{200,200};first.command=StickyReferenceCommand{semantic::StickyContent{200,200},a,true};}
  if(kind==2){first.record.kind=semantic::ObjectKind::kTable;first.record.kindVersion=1;semantic::TableContent t;first.record.content=t;first.command=TableReferenceCommand{t,a,0};}
  first.record.textLayout=a;first.record.worldBounds={0,0,120,80};
  auto initial=manager.update({&first,1},{0,0,900,700},1);
  const auto revision=manager.find(first.record.objectId)->revisions;
  auto equal=first;auto b=std::make_shared<text::TextLayoutSnapshot>(*a);equal.record.textLayout=b;
  std::visit([&](auto& c){if constexpr(requires{c.labelLayout;})c.labelLayout=b;else if constexpr(requires{c.layout;})c.layout=b;},equal.command);
  auto cloned=manager.update({&equal,1},{0,0,900,700},1);auto after=manager.find(first.record.objectId)->revisions;
  std::cout<<"{\"kind\":"<<kind<<",\"layout_digest_equal\":"<<(a->digest==b->digest)<<",\"distinct_pointer\":"<<(a.get()!=b.get())<<",\"equal_values_produce_damage\":"<<cloned.contributions.size()<<",\"content_revision_before\":"<<revision.content<<",\"content_revision_after\":"<<after.content<<"}\n";
 }
}
