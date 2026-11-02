#pragma once
#include "native_session.hpp"
#include <filesystem>
namespace canvas::sdk {std::unique_ptr<NativeSessionPlatform> makeWindowsNativePlatform(const NativeView&,const std::filesystem::path&);}
