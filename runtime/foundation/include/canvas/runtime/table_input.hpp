#pragma once
#include "canvas/runtime/text_input.hpp"
namespace canvas::runtime {
enum class TableAction : std::uint8_t { kCreate,kSelect,kInsertRow,kInsertColumn,kRemoveRow,kRemoveColumn,kRowHeight,kColumnWidth,kCellStyle,kGridStyle,kNextCell,kPreviousCell };
struct TableRequest final {
  TableAction action=TableAction::kCreate;
  foundation::ObjectId object{},row{},column{};
  std::uint64_t generation=0;
  std::uint32_t rows=3,columns=3,index=0;
  double x=0,y=0,size=48;
  std::optional<std::array<float,4>> fill;
  TextFormatPatch format{};
  std::optional<std::array<float,4>> gridColor;
  std::optional<double> gridWidth;
};
struct TableResult final {
  bool applied=false;
  foundation::ObjectId object{};
  std::uint64_t generation=0;
};
struct TableSnapshot final {
  bool available=false,editable=false;
  foundation::ObjectId object{},row{},column{};
  semantic::TableContent content{};
  std::uint64_t generation=0;
};
}
