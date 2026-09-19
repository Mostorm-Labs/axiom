#include "arc/arc.hpp"

#include <memory>

extern "C" void* axiom_ink_android_create_input_source() {
  return arc::CreateAndroidInputSource().release();
}

extern "C" void axiom_ink_android_destroy_input_source(void* source) {
  delete static_cast<arc::InputSource*>(source);
}
