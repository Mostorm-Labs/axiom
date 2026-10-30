#include "native_platform.hpp"

#include <atomic>
#include <axiom/api.h>
#include <canvas/input/platform_input_contract.hpp>
#include <cstring>
#include <map>
#include <mutex>
#include <objbase.h>
#include <set>
#include <thread>

namespace {
using canvas::sdk::Json;
struct Context {
    std::thread::id thread = std::this_thread::get_id();
    bool busy = false;
    axiom_options options{};
    std::unique_ptr<canvas::sdk::Session> session;
    std::set<uint64_t> sources;
    std::map<std::pair<uint64_t, uint64_t>, canvas::input::PointerKey> contacts;
    canvas::sdk::NativePlatform& platform() {
        return static_cast<canvas::sdk::NativePlatform&>(session->platform());
    }
    ~Context() {
        if (session) {
            session->clearCallbacks();
            session->platform().release();
        }
    }
    static void operation(void* p, const uint8_t* data, size_t n) noexcept {
        auto& c = *static_cast<Context*>(p);
        const bool before = c.busy;
        c.busy = true;
        c.platform().inCallback(true);
        if (c.options.on_operation)
            try {
                c.options.on_operation(c.options.callback_context, data, n);
            } catch (...) {
                error(p);
            }
        c.platform().inCallback(false);
        c.busy = before;
    }
    static void error(void* p) noexcept {
        auto& c = *static_cast<Context*>(p);
        const bool before = c.busy;
        c.busy = true;
        if (c.options.on_error)
            try {
                c.options.on_error(c.options.callback_context, AXIOM_INTERNAL_ERROR);
            } catch (...) {
            }
        c.busy = before;
    }
};
std::mutex mutex;
std::map<uint64_t, std::shared_ptr<Context>> runtimes;
std::map<uint64_t, std::vector<uint8_t>> buffers;
std::atomic<uint64_t> next{1};
uint64_t put(std::vector<uint8_t> data) {
    std::lock_guard lock(mutex);
    auto id = next++;
    buffers.emplace(id, std::move(data));
    return id;
}
uint64_t put(std::string text) {
    return put(std::vector<uint8_t>(text.begin(), text.end()));
}
std::shared_ptr<Context> lookup(uint64_t id, int& code) {
    std::lock_guard lock(mutex);
    auto it = runtimes.find(id);
    if (it == runtimes.end()) {
        code = AXIOM_INVALID_HANDLE;
        return nullptr;
    }
    // A rejected foreign thread must not become the last Context owner during
    // concurrent close/abandon and destroy a thread-affine Runtime there.
    if (it->second->thread != std::this_thread::get_id()) {
        code = AXIOM_WRONG_THREAD;
        return nullptr;
    }
    code = AXIOM_OK;
    return it->second;
}
int status(const std::shared_ptr<Context>& c) {
    if (!c)
        return AXIOM_INVALID_HANDLE;
    if (c->thread != std::this_thread::get_id())
        return AXIOM_WRONG_THREAD;
    if (c->busy)
        return AXIOM_REENTRANT;
    return AXIOM_OK;
}
struct Guard {
    Context& c;
    explicit Guard(Context& value) : c(value) {
        c.busy = true;
    }
    ~Guard() {
        c.busy = false;
    }
};
std::filesystem::path assetRoot(const wchar_t* root) {
    if (root) {
        auto p = std::filesystem::path(root);
        if (!p.is_absolute())
            throw std::invalid_argument("asset_root must be absolute");
        return p;
    }
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&axiom_create),
                            &module))
        throw std::runtime_error("module path");
    std::wstring path(32768, L'\0');
    auto n = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!n || n == path.size())
        throw std::runtime_error("module path");
    path.resize(n);
    return std::filesystem::path(path).parent_path() / L"share" / L"axiom";
}
template <class F> int checked(uint64_t id, F action) noexcept {
    try {
        int code = AXIOM_OK;
        auto c = lookup(id, code);
        if (code)
            return code;
        code = status(c);
        if (code)
            return code;
        Guard guard(*c);
        return action(*c);
    } catch (const std::bad_alloc&) {
        return AXIOM_OUT_OF_MEMORY;
    } catch (const std::invalid_argument&) {
        return AXIOM_INVALID_ARGUMENT;
    } catch (...) {
        return AXIOM_INTERNAL_ERROR;
    }
}
canvas::input::PlatformPointerBatch batch(const axiom_sample* samples, size_t n) {
    if (!samples || !n || n > 4096)
        throw std::invalid_argument("samples");
    canvas::input::PlatformPointerBatch b;
    b.samples.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& s = samples[i];
        if (s.phase > 3 || s.tool > 3 || s.flags > 31 || s.provenance > 2 || s.modifiers > 1 ||
            s.source != samples[0].source || s.pointer != samples[0].pointer)
            throw std::invalid_argument("sample");
        canvas::input::PlatformPointerSample p;
        p.source = s.source;
        p.pointer = s.pointer;
        p.sequence = s.sequence;
        p.timestampNs = s.timestamp_ns;
        p.x = s.x;
        p.y = s.y;
        p.pressure = s.pressure;
        p.tiltX = s.tilt_x;
        p.tiltY = s.tilt_y;
        p.contact = {s.width, s.height, s.orientation, (s.flags & 16) != 0};
        p.capabilities = {static_cast<canvas::input::PointerTool>(s.tool),
                          (s.flags & 1) != 0,
                          (s.flags & 2) != 0,
                          (s.flags & 4) != 0,
                          (s.flags & 8) != 0,
                          (s.flags & 16) != 0};
        p.phase = static_cast<canvas::input::PointerPhase>(s.phase);
        p.provenance = static_cast<canvas::input::SampleProvenance>(s.provenance);
        p.modifiers = static_cast<canvas::input::PointerModifiers>(s.modifiers);
        b.samples.push_back(p);
    }
    return b;
}
bool contact(Context& c,
             uint64_t handle,
             const axiom_contact* key,
             canvas::input::PointerKey& out) {
    if (!key || key->runtime != handle)
        return false;
    auto it = c.contacts.find({key->source, key->pointer});
    if (it == c.contacts.end() || it->second.generation != key->generation)
        return false;
    out = it->second;
    return true;
}
} // namespace
extern "C" {
uint32_t axiom_abi_version(void) {
    return AXIOM_ABI_VERSION;
}
int axiom_create(const axiom_options* options, axiom_runtime* out, axiom_buffer* error) {
    if (out)
        *out = 0;
    if (error)
        *error = 0;
    try {
        if (!options || !out || options->size != sizeof(axiom_options) ||
            options->abi_version != AXIOM_ABI_VERSION ||
            !IsWindow(static_cast<HWND>(options->parent_window)) ||
            GetWindowThreadProcessId(static_cast<HWND>(options->parent_window), nullptr) !=
                GetCurrentThreadId() ||
            options->snapshot_size > 134217728 || (!options->snapshot && options->snapshot_size))
            return AXIOM_INVALID_ARGUMENT;
        auto c = std::make_shared<Context>();
        c->options = *options;
        c->session = std::make_unique<canvas::sdk::Session>(
            canvas::sdk::makeNativePlatform(static_cast<HWND>(options->parent_window),
                                            assetRoot(options->asset_root)),
            canvas::sdk::SessionCallbacks{c.get(), Context::operation, Context::error});
        Json j = options->open_json ? Json::parse(options->open_json) : Json::object();
        j["command"] = "open";
        if (!j.contains("documentId")) {
            GUID id{};
            if (FAILED(CoCreateGuid(&id)))
                return AXIOM_INTERNAL_ERROR;
            const auto* b = reinterpret_cast<const uint8_t*>(&id);
            std::string text;
            constexpr char h[] = "0123456789abcdef";
            for (size_t i = 0; i < 16; ++i) {
                text += h[b[i] >> 4];
                text += h[b[i] & 15];
            }
            j["documentId"] = text;
        }
        c->busy = true;
        auto reply = c->session->call(j.dump(), {options->snapshot, options->snapshot_size});
        c->busy = false;
        if (!Json::parse(reply).value("ok", false)) {
            if (error)
                *error = put(std::move(reply));
            return AXIOM_REJECTED;
        }
        std::lock_guard lock(mutex);
        auto id = next++;
        runtimes.emplace(id, std::move(c));
        *out = id;
        return AXIOM_OK;
    } catch (const std::bad_alloc&) {
        return AXIOM_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        if (error)
            try {
                *error = put(std::string(e.what()));
            } catch (...) {
            }
        return AXIOM_INVALID_ARGUMENT;
    } catch (...) {
        return AXIOM_INTERNAL_ERROR;
    }
}
int axiom_call(axiom_runtime id,
               const char* data,
               size_t size,
               const uint8_t* bytes,
               size_t count,
               axiom_buffer* out,
               axiom_buffer* binary) {
    if (out)
        *out = 0;
    if (binary)
        *binary = 0;
    if (!out || !data || !size || size > 1048576 || count > 134217728 || (!bytes && count))
        return AXIOM_INVALID_ARGUMENT;
    return checked(id, [&](Context& c) -> int {
        auto j = Json::parse(data, data + size);
        const auto cmd = j.at("command").get<std::string>();
        try {
            canvas::sdk::validateTransportEnums(j);
        } catch (const std::exception& e) {
            *out =
                put(Json{{"ok", false}, {"code", "INVALID_REQUEST"}, {"message", e.what()}}.dump());
            return AXIOM_OK;
        }
        if (cmd == "open" || cmd == "close" || cmd == "pointer")
            return AXIOM_INVALID_ARGUMENT;
        if (!c.contacts.empty() && (cmd == "resize" || cmd == "lose"))
            return AXIOM_BUSY;
        const bool format = cmd == "text" && j.value("action", std::string{}) == "intent" &&
                            j.contains("intent") &&
                            j["intent"].value("kind", 0) ==
                                static_cast<int>(canvas::runtime::TextIntentKind::kFormat);
        const auto action = j.contains("action") && j["action"].is_string()
                                ? j["action"].get<std::string>()
                                : std::string{};
        const bool editing = cmd == "version" || cmd == "text" || cmd == "textEdit" ||
                             cmd == "state" || cmd == "frame" || cmd == "selection" ||
                             cmd == "hover" || cmd == "readSection" || cmd == "presets" ||
                             action == "read" || action == "readCrop" || action == "token" ||
                             action == "receipt";
        if ((!editing || format) && !c.platform().finishText(format))
            return AXIOM_BUSY;
        auto reply = c.session->call(std::string_view(data, size), {bytes, count});
        if (cmd == "state" && !c.contacts.empty()) {
            auto v = Json::parse(reply);
            if (v.value("ok", false)) {
                v["value"]["activeContacts"] =
                    v["value"]["activeContacts"].get<size_t>() + c.contacts.size();
                reply = v.dump();
            }
        }
        uint64_t b = 0;
        try {
            if (binary)
                b = put(std::vector<uint8_t>(c.session->bytes().begin(), c.session->bytes().end()));
            *out = put(std::move(reply));
            if (binary)
                *binary = b;
        } catch (...) {
            if (b)
                axiom_buffer_release(b);
            Context::error(&c);
            throw;
        }
        return AXIOM_OK;
    });
}
int axiom_window(axiom_runtime id, void** out) {
    if (!out)
        return AXIOM_INVALID_ARGUMENT;
    *out = nullptr;
    return checked(id, [&](Context& c) -> int {
        *out = c.platform().window();
        return *out ? AXIOM_OK : AXIOM_REJECTED;
    });
}
int axiom_close(axiom_runtime id) {
    return checked(id, [&](Context& c) -> int {
        if (!c.contacts.empty() || c.platform().activeContacts() || !c.platform().finishText(false))
            return AXIOM_BUSY;
        if (!Json::parse(c.session->call("{\"command\":\"close\"}")).value("ok", false))
            return AXIOM_REJECTED;
        c.session->clearCallbacks();
        std::lock_guard lock(mutex);
        runtimes.erase(id);
        return AXIOM_OK;
    });
}
int axiom_abandon(axiom_runtime id) {
    return checked(id, [&](Context& c) -> int {
        c.session->clearCallbacks();
        c.session->platform().release();
        std::lock_guard lock(mutex);
        runtimes.erase(id);
        return AXIOM_OK;
    });
}
int axiom_buffer_size(axiom_buffer id, size_t* n) {
    if (!n)
        return AXIOM_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    auto it = buffers.find(id);
    if (it == buffers.end())
        return AXIOM_INVALID_HANDLE;
    *n = it->second.size();
    return AXIOM_OK;
}
int axiom_buffer_copy(axiom_buffer id, void* p, size_t n, size_t* written) {
    std::lock_guard lock(mutex);
    auto it = buffers.find(id);
    if (it == buffers.end())
        return AXIOM_INVALID_HANDLE;
    if (written)
        *written = it->second.size();
    if (n < it->second.size())
        return AXIOM_BUFFER_TOO_SMALL;
    if (!p && !it->second.empty())
        return AXIOM_INVALID_ARGUMENT;
    if (!it->second.empty())
        std::memcpy(p, it->second.data(), it->second.size());
    return AXIOM_OK;
}
int axiom_buffer_release(axiom_buffer id) {
    std::lock_guard lock(mutex);
    return buffers.erase(id) ? AXIOM_OK : AXIOM_INVALID_HANDLE;
}
int axiom_input_source(axiom_runtime id, uint64_t source, int add) {
    return checked(id, [&](Context& c) -> int {
        if (source < 0x100000000ULL)
            return AXIOM_INVALID_ARGUMENT;
        auto& owner = *c.session->composition();
        if (add) {
            auto [entry, added] = c.sources.insert(source);
            if (!added)
                return AXIOM_REJECTED;
            if (!owner.registerSource(source)) {
                c.sources.erase(entry);
                return AXIOM_REJECTED;
            }
        } else {
            for (auto [k, v] : c.contacts) {
                (void)v;
                if (k.first == source)
                    return AXIOM_BUSY;
            }
            if (!c.sources.contains(source) ||
                !owner.unregisterSource(source, 1, GetTickCount64() * 1000000))
                return AXIOM_REJECTED;
            c.sources.erase(source);
        }
        return AXIOM_OK;
    });
}
int axiom_input_begin(axiom_runtime id, const axiom_sample* s, size_t n, axiom_contact* out) {
    if (!out)
        return AXIOM_INVALID_ARGUMENT;
    *out = {};
    return checked(id, [&](Context& c) -> int {
        auto b = batch(s, n);
        if (!c.sources.contains(s->source) || c.contacts.contains({s->source, s->pointer}))
            return AXIOM_REJECTED;
        auto entry =
            c.contacts.emplace(std::make_pair(s->source, s->pointer), canvas::input::PointerKey{})
                .first;
        auto key = c.session->composition()->beginContact(b);
        if (!key) {
            c.contacts.erase(entry);
            return AXIOM_REJECTED;
        }
        entry->second = *key;
        *out = {id, key->source, key->pointer, key->generation};
        return AXIOM_OK;
    });
}
int axiom_input_submit(axiom_runtime id, const axiom_contact* t, const axiom_sample* s, size_t n) {
    return checked(id, [&](Context& c) -> int {
        canvas::input::PointerKey key;
        if (!contact(c, id, t, key))
            return AXIOM_REJECTED;
        auto b = batch(s, n);
        if (b.samples.front().source != key.source || b.samples.front().pointer != key.pointer)
            return AXIOM_REJECTED;
        if (!c.session->composition()->submitContact(key, b))
            return AXIOM_REJECTED;
        if (b.samples.back().phase == canvas::input::PointerPhase::kUp ||
            b.samples.back().phase == canvas::input::PointerPhase::kCancel)
            c.contacts.erase({key.source, key.pointer});
        return AXIOM_OK;
    });
}
int axiom_input_cancel(axiom_runtime id, const axiom_contact* t, uint64_t seq, uint64_t time) {
    return checked(id, [&](Context& c) -> int {
        canvas::input::PointerKey key;
        if (!contact(c, id, t, key) || !c.session->composition()->cancelContact(key, seq, time))
            return AXIOM_REJECTED;
        c.contacts.erase({key.source, key.pointer});
        return AXIOM_OK;
    });
}
}
