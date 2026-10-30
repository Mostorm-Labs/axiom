#define NOMINMAX
#include <axiom/axiom.hpp>
#include <cstdio>
#include <windows.h>
using J = axiom::Json;
static uint64_t number(std::span<const uint8_t> b, size_t& p) {
    uint64_t v = 0;
    for (unsigned s = 0; s < 64; s += 7) {
        if (p >= b.size())
            throw std::runtime_error("short varint");
        const auto x = b[p++];
        v |= uint64_t(x & 127) << s;
        if (!(x & 128))
            return v;
    }
    throw std::runtime_error("varint");
}
static std::vector<uint8_t> field(std::span<const uint8_t> b) {
    size_t p = 0;
    while (p < b.size()) {
        const auto tag = number(b, p);
        if ((tag & 7) == 0) {
            (void)number(b, p);
            continue;
        }
        if ((tag & 7) != 2)
            throw std::runtime_error("wire type");
        const auto n = number(b, p);
        if (n > b.size() - p)
            throw std::runtime_error("short field");
        if ((tag >> 3) == 1)
            return {b.begin() + p, b.begin() + p + n};
        p += n;
    }
    throw std::runtime_error("missing identity");
}
static std::string operationId(std::span<const uint8_t> b) {
    auto nested = field(b);
    auto id = field(nested);
    if (id.size() != 16)
        throw std::runtime_error("Id128 size");
    std::string text;
    for (auto v : id) {
        text += "0123456789abcdef"[v >> 4];
        text += "0123456789abcdef"[v & 15];
    }
    return text;
}
int main() {
    const auto w = CreateWindowExW(0,
                                   L"STATIC",
                                   L"Published SDK recovery probe",
                                   WS_OVERLAPPEDWINDOW,
                                   0,
                                   0,
                                   700,
                                   500,
                                   nullptr,
                                   nullptr,
                                   GetModuleHandleW(nullptr),
                                   nullptr);
    try {
        std::vector<uint8_t> first, second;
        axiom::Options a{w};
        a.open = {{"documentId", "1234567890abcdef1234567890abcdef"}};
        a.onOperation = [&](std::span<const uint8_t> b) { first.assign(b.begin(), b.end()); };
        axiom::Runtime source(a);
        source.createSticky({{"objectId", "0000000000000000000000000000aa01"}, {"size", 80}});
        auto saved = source.snapshot({{"generation", source.state().at("generation")},
                                      {"documentRevision", "1"},
                                      {"recoveryFrontier", {8, 1, 16, 1, 24, 1}}});
        axiom::Options b{w};
        b.open = {{"documentId", source.state().at("documentId")},
                  {"restore",
                   {{"documentRevision", "1"},
                    {"recoveryFrontier", {8, 1, 16, 1, 24, 1}},
                    {"checkpointDigest", saved.value.at("checkpointDigest")},
                    {"resourceBinding", "55"}}}};
        b.snapshot = saved.bytes;
        b.onOperation = [&](std::span<const uint8_t> bytes) {
            second.assign(bytes.begin(), bytes.end());
        };
        axiom::Runtime recovered(b);
        recovered.createSticky(
            {{"objectId", "0000000000000000000000000000aa02"}, {"size", 80}, {"x", 100}});
        const auto id1 = operationId(first), id2 = operationId(second);
        const bool collision = id1 == id2;
        const J result = {{"build", source.buildInfo()},
                          {"beforeSnapshotOperationId", id1},
                          {"afterRestoreOperationId", id2},
                          {"differentOperationsSameId", collision},
                          {"differentWirePayloads", first != second},
                          {"objectsAfterRestoreAndEdit", recovered.state().at("objects")},
                          {"obligation", "OperationId must not repeat across checkpoint restore"},
                          {"result", collision ? "FAIL" : "PASS"}};
        if (!recovered.close() || !source.close())
            throw std::runtime_error("close");
        std::puts(result.dump(2).c_str());
        DestroyWindow(w);
        return collision ? 1 : 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        DestroyWindow(w);
        return 2;
    }
}
