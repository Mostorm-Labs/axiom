#pragma once

#include "canvas/debug_ui/control_router.hpp"
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cwctype>
#include <array>
#include <span>

namespace canvas::minimal_runtime_host {
inline std::string imageMimeTypeFromBytes(std::span<const std::uint8_t> bytes) {
  constexpr std::array<std::uint8_t, 8> png{{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a}};
  if (bytes.size() >= png.size() && std::equal(png.begin(), png.end(), bytes.begin()))
    return "image/png";
  if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff)
    return "image/jpeg";
  if (bytes.size() >= 12 && std::equal(bytes.begin(), bytes.begin() + 4, "RIFF") &&
      std::equal(bytes.begin() + 8, bytes.begin() + 12, "WEBP"))
    return "image/webp";
  return {};
}

inline std::string imageMimeTypeFromExtension(const std::filesystem::path& path) {
  auto extension = path.extension().wstring();
  std::transform(extension.begin(), extension.end(), extension.begin(),
      [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
  if (extension == L".png") return "image/png";
  if (extension == L".jpg" || extension == L".jpeg") return "image/jpeg";
  if (extension == L".webp") return "image/webp";
  return {};
}

inline debug_ui::ImageFileResult chooseWindowsImageFile(HWND owner) {
  std::array<wchar_t, 32768> path{};
  OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = owner;
  // Keep individual filters visible in Explorer and include an explicit fallback
  // for files whose extension is hidden, duplicated, or otherwise non-standard.
  dialog.lpstrFilter =
      L"Image files (validated PNG/JPEG/WebP)\0*.*\0"
      L"PNG files (*.png)\0*.png\0"
      L"JPEG files (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
      L"WebP files (*.webp)\0*.webp\0"
      L"All files (*.*)\0*.*\0\0";
  // Browse all files by default; actual encoded bytes are checked below and
  // decoded by Runtime. Do not hide candidates based only on their suffix.
  dialog.nFilterIndex = 1;
  dialog.lpstrFile = path.data(); dialog.nMaxFile = static_cast<DWORD>(path.size());
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
  if (!GetOpenFileNameW(&dialog)) {
    debug_ui::ImageFileResult out;
    if (CommDlgExtendedError() == 0) out.cancelled = true;
    else out.error = "Image file picker failed";
    return out;
  }
  const std::filesystem::path filePath(path.data());
  debug_ui::ImageFileResult out;
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file) { out.error = "Image file could not be opened"; return out; }
  const auto size = file.tellg();
  if (size <= 0 || size > 128 * 1024 * 1024) { out.error = "Image file is empty or exceeds 128 MiB"; return out; }
  out.bytes.resize(static_cast<std::size_t>(size)); file.seekg(0);
  if (!file.read(reinterpret_cast<char*>(out.bytes.data()), static_cast<std::streamsize>(out.bytes.size()))) {
    out.bytes.clear(); out.error = "Image file read failed";
    return out;
  }
  const auto extensionMime = imageMimeTypeFromExtension(filePath);
  const auto contentMime = imageMimeTypeFromBytes(out.bytes);
  if (contentMime.empty()) {
    out.bytes.clear(); out.error = "Selected file is not a supported PNG, JPEG or WebP image";
    return out;
  }
  if (!extensionMime.empty() && extensionMime != contentMime) {
    out.bytes.clear(); out.error = "File extension does not match its image data";
    return out;
  }
  out.mimeType = contentMime;
  return out;
}
} // namespace canvas::minimal_runtime_host
