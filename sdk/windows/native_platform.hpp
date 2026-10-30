#pragma once
#include "session.hpp"

#include <filesystem>
#include <windows.h>
namespace canvas::sdk {
class NativePlatform : public SessionPlatform {
  public:
    virtual HWND window() const noexcept = 0;
    virtual bool finishText(bool keepEditing) = 0;
    virtual void inCallback(bool) = 0;
};
std::unique_ptr<NativePlatform> makeNativePlatform(HWND, const std::filesystem::path&);
} // namespace canvas::sdk
