#include "ink_playground_host.hpp"
#if defined(AXIOM_G47_BUNDLED_RESOURCES)
#include "g47_latin_font.hpp"
#include "g47_cjk_font.hpp"
#endif

namespace canvas::ink_playground {
bool InkPlaygroundHost::configureBundledTextResources() {
  if(textLayoutService_) return true;
#if defined(AXIOM_G47_BUNDLED_RESOURCES)
  return configureTextResources({std::begin(kG47LatinFont),std::end(kG47LatinFont)},
      {std::begin(kG47CjkFont),std::end(kG47CjkFont)},text::LayoutContext{128,1});
#else
  return false;
#endif
}
}
