#pragma once
#include <canvas/runtime/runtime_composition.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <span>
#include <string>

namespace canvas::sdk {
using Json = nlohmann::json;
// Platform realization only. Canonical, input, text and History remain owned
// by the borrowed RuntimeComposition and its facade.
class SessionPlatform {
  public:
    virtual ~SessionPlatform() = default;
    virtual Json open(runtime::RuntimeComposition&, const Json&) = 0;
    virtual bool loadFonts() = 0;
    virtual bool resize(float, float, float) = 0;
    virtual bool lose() = 0;
    virtual Json frame(bool) = 0;
    virtual bool shutdown() = 0;
    virtual void release() noexcept = 0;
    virtual std::uint64_t nowNs() const noexcept = 0;
    virtual std::size_t activeContacts() const noexcept {
        return 0;
    }
};
struct SessionCallbacks {
    void* context = nullptr;
    void (*operation)(void*, const std::uint8_t*, std::size_t) noexcept = nullptr;
    void (*deliveryError)(void*) noexcept = nullptr;
};
// Private SDK transport adapter, never installed as a public C++ binary ABI.
class Session final {
  public:
    explicit Session(std::unique_ptr<SessionPlatform>, SessionCallbacks = {});
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    std::string call(std::string_view, std::span<const std::uint8_t> = {});
    std::span<const std::uint8_t> bytes() const noexcept;
    runtime::RuntimeComposition* composition() noexcept;
    SessionPlatform& platform() noexcept;
    void activateTextAccess() noexcept; // one module-local Web instance only
    void clearCallbacks() noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace canvas::sdk
