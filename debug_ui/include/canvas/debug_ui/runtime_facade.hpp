#pragma once
#include "canvas/debug_ui/command.hpp"
#include "canvas/debug_ui/snapshot.hpp"
#include <cstdint>
#include <optional>
namespace canvas::debug_ui {
enum class ProductDebugAction : std::uint8_t { kSetTool, kSetCamera, kUndo, kRedo };
struct DebugControl final { ProductDebugAction action=ProductDebugAction::kSetTool; std::uint64_t requestId=0; std::uint64_t generation=0; std::uint64_t deadlineSequence=0; };
class RuntimeFacade {
 public:
  virtual ~RuntimeFacade() = default;
  virtual DebugSnapshot readDiagnostics() const = 0;
  virtual std::optional<CommandReceipt> submitDebugCommand(DebugCommand command) = 0;
  virtual std::optional<CommandReceipt> submitProductControl(DebugControl control) = 0;
};
} // namespace canvas::debug_ui
