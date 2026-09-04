#include "g1_06_digest.hpp"

namespace canvas::verification::g1_06 {

std::uint64_t digestCanonicalProjectionBytes(
    std::string_view bytes) noexcept {
    std::uint64_t digest = 0xcbf29ce484222325ULL;
    for (const unsigned char byte : bytes) {
        digest ^= static_cast<std::uint64_t>(byte);
        digest *= 0x100000001b3ULL;
    }
    return digest;
}

} // namespace canvas::verification::g1_06
