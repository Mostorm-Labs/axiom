#include "canvas/input/platform_normalization.hpp"

#include <cassert>

int main() {
  const auto windows = canvas::input::normalizePlatformPointer(
      100, 7, 1, canvas::input::PointerTool::kPen, true, false, {2.0F, 3.0F, 0.0F, true});
  const auto android = canvas::input::normalizePlatformPointer(
      200, 3, 4, canvas::input::PointerTool::kEraser, true, true, {5.0F, 4.0F, 0.0F, true});
  const auto web = canvas::input::normalizePlatformPointer(
      300, 9, 2, canvas::input::PointerTool::kFinger, false, false, {10.0F, 10.0F, 0.0F, true});
  assert(windows.key.valid() && windows.capabilities.pressure);
  assert(android.capabilities.hardwareEraser && android.capabilities.tool == canvas::input::PointerTool::kEraser);
  assert(web.capabilities.tool == canvas::input::PointerTool::kFinger);
  const auto positionOnly = canvas::input::normalizePlatformPointer(
      400, 4, 1, canvas::input::PointerTool::kUnknown, false, false, {});
  assert(!positionOnly.capabilities.contactGeometry);
  assert(!positionOnly.contact.available);
  return 0;
}
