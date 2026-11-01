#pragma once
#include "canvas/semantic/object_record.hpp"
#include <array>
namespace canvas::semantic {
// Runtime/developer value helpers. Consumer SDKs expose only bounded DTOs.
inline double tableWidth(const TableContent& t) { double n=0;for(const auto& a:t.columns)n+=a.size;return n; }
inline double tableHeight(const TableContent& t) { double n=0;for(const auto& a:t.rows)n+=a.size;return n; }
inline std::optional<std::size_t> tableCellIndex(const TableContent& t,ObjectId row,ObjectId column) {
  for(std::size_t r=0;r<t.rows.size();++r)if(t.rows[r].id==row)
    for(std::size_t c=0;c<t.columns.size();++c)if(t.columns[c].id==column)return r*t.columns.size()+c;
  return {};
}
inline std::array<double,4> tableCellRect(const TableContent& t,std::size_t index) {
  const auto r=index/t.columns.size(),c=index%t.columns.size();double x=0,y=0;
  for(std::size_t i=0;i<c;++i)x+=t.columns[i].size;
  for(std::size_t i=0;i<r;++i)y+=t.rows[i].size;
  return {x,y,x+t.columns[c].size,y+t.rows[r].size};
}
[[nodiscard]] bool prepareTableTextDelta(const TableContent&,ObjectId row,ObjectId column,const RichTextDelta&,TableContent&);
// A derived layout value only: never inserted into ObjectStore or given a
// separate canonical identity. The text session retains table/row/column IDs.
inline ObjectRecord tableTextView(const ObjectRecord& record,std::size_t index) {
  auto out=record;const auto& table=std::get<TableContent>(record.content);
  const auto rect=tableCellRect(table,index);const auto& m=record.transform;
  out.transform.tx+=m.a*(rect[0]+6)+m.c*(rect[1]+6);
  out.transform.ty+=m.b*(rect[0]+6)+m.d*(rect[1]+6);
  out.kind=ObjectKind::kRichText;out.kind_version=3;out.content=table.cells[index].text;return out;
}
}
