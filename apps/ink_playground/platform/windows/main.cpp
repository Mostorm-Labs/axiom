// Windows entrypoint is intentionally a thin native Arc composition shell.
// WM_POINTER/coalesced history is forwarded to Arc::InputSource; production
// brush and interaction semantics are not reimplemented here.
#include "arc/arc.hpp"

int main() {
  auto source = arc::CreateWindowsInputSource();
  return source == nullptr ? 1 : 0;
}
