#pragma once
#include "canvas/semantic/snapshot.hpp"
#include "protobuf_codec_internal.hpp"
#include <type_traits>
#if defined(CANVAS_SEMANTIC_PROTOBUF)
namespace canvas::semantic::internal {
// Allocation-free lower-bound admission on domain values. The exact measured
// canonical size is checked on each mapped object and on the complete message.
// This pass prevents an unbounded source from being deep-copied into Protobuf.
struct SnapshotSourceScan {
 std::size_t bytes=0,limit=kSnapshotEnvelopeBytes;
 bool add(std::size_t n){if(n>limit-bytes)return false;bytes+=n;return true;}
 bool count(std::size_t n){return n<=65535;}
 bool text(const RichTextContent& t){
  if(!count(t.document.paragraphs.size()))return false;
  for(const auto& p:t.document.paragraphs){if(!add(16)||!count(p.runs.size()))return false;for(const auto& r:p.runs)if(!add(r.text.size()))return false;}
  return true;
 }
 bool path(const VectorPathGeometry& p){return count(p.commands.size())&&add(p.commands.size());}
 bool resource(const ResourceManifestEntry& e){return add(e.content_hash.size())&&add(e.metadata.size());}
 bool object(const ObjectRecord& o){
  if(!count(o.erase_masks.size())||!count(o.properties.entries.size())||!add(o.placement.order_key.bytes().size()))return false;
  for(const auto& e:o.properties.entries)if(const auto* style=std::get_if<StrokeStyleValue>(&e.value))if(const auto* solid=std::get_if<SolidStroke>(style))if(const auto* dash=std::get_if<DashPattern>(&solid->dash))if(!count(dash->segments.size())||!add(dash->segments.size()*8))return false;
  for(const auto& m:o.erase_masks){if(!add(16))return false;if(const auto* s=std::get_if<SweptCircleMask>(&m.geometry)){if(!count(s->segments.size())||!add(s->segments.size()*80))return false;}else if(!path(std::get<FilledPathMask>(m.geometry).path))return false;}
  return std::visit([&](const auto& c){using T=std::decay_t<decltype(c)>;
   if constexpr(std::is_same_v<T,RichTextContent>)return text(c);
   else if constexpr(std::is_same_v<T,ShapeContent>)return !c.label||text(*c.label);
   else if constexpr(std::is_same_v<T,TableContent>){if(!count(c.rows.size())||!count(c.columns.size())||!count(c.cells.size()))return false;for(const auto& cell:c.cells)if(!text(cell.text))return false;return true;}
   else if constexpr(std::is_same_v<T,SectionContent>)return add(c.title.size());
   else if constexpr(std::is_same_v<T,VectorPathContent>)return path(c.geometry);
   else if constexpr(std::is_same_v<T,CompactStrokeContent>)return c.stroke.knots.size()<=262144&&add(c.stroke.knots.size()*18);
   else if constexpr(std::is_same_v<T,VectorStrokeContent>||std::is_same_v<T,DabStrokeContent>)return std::visit([&](const auto& d){if constexpr(requires{d.samples;})return count(d.samples.size())&&add(d.samples.size());else return count(d.dabs.size())&&add(d.dabs.size());},c.stroke.data);
   else if constexpr(std::is_same_v<T,BrushStrokeContent>||std::is_same_v<T,DabBrushStrokeContent>){if(!count(c.stroke.confirmed_samples.size())||!add(c.stroke.confirmed_samples.size())||!count(c.stroke.snapshot.stages.size())||!count(c.stroke.snapshot.resources.size()))return false;for(const auto& r:c.stroke.snapshot.resources)if(!add(r.content_sha256.size()))return false;if constexpr(requires{c.stroke.vector_output;})return count(c.stroke.vector_output.outline.size())&&add(c.stroke.vector_output.outline.size());else return count(c.stroke.dab_output.dabs.size())&&add(c.stroke.dab_output.dabs.size());}
   else return true;
  },o.content);
 }
};
inline SemanticError preflightSnapshotSource(const SemanticSnapshot& s){
 SnapshotSourceScan scan;
 if(!scan.count(s.objects.size())||!scan.count(s.manifest.entries.size())||!scan.count(s.manifest.retired_entries.size())||!scan.add(s.recovery_frontier.size())||!scan.add(s.capability_requirements.size())||!scan.add(s.document_digest.size())||!scan.add(s.checkpoint_digest.size()))return SemanticError::kLimitExceeded;
 for(const auto& e:s.manifest.entries)if(!scan.resource(e))return SemanticError::kLimitExceeded;
 for(const auto& e:s.manifest.retired_entries)if(!scan.resource(e.preimage)||!scan.add(e.last_content_hash.size())||!scan.add(e.lineage_token.size()))return SemanticError::kLimitExceeded;
 for(const auto& o:s.objects){SnapshotSourceScan object;object.limit=16U*1024U*1024U;if(!object.object(o)||!scan.add(object.bytes))return SemanticError::kLimitExceeded;}
 return SemanticError::kNone;
}
}
#endif
