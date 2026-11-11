#pragma once
#include <filesystem>
namespace canvas::runtime {class RuntimeCompositionCore;}
namespace canvas::minimal_runtime_host {
inline bool bindHostTextFonts(runtime::RuntimeCompositionCore&,const std::filesystem::path&,const std::filesystem::path&){return false;}
}
