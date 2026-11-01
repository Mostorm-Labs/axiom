#include "canvas/semantic/table_content.hpp"
#include "rich_text_state_plan.hpp"
#include "object_record_semantics_internal.hpp"
namespace canvas::semantic {
bool prepareTableTextDelta(const TableContent& before,ObjectId row,ObjectId column,const RichTextDelta& delta,TableContent& next){
  const auto index=tableCellIndex(before,row,column);if(!index)return false;next=before;
  if(!internal::prepareRichTextDeltaState(before.cells[*index].text,delta,&next.cells[*index].text).ok())return false;
  next.cells[*index].text.layout=before.cells[*index].text.layout;
  return internal::normalizeTableContent(next)&&internal::validateTableContent(next);
}
}
