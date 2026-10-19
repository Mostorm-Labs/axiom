#include "sha256_internal.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <vector>

namespace canvas::semantic::internal {
namespace {
constexpr std::array<std::uint32_t, 64> kK = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) noexcept { return (x >> n) | (x << (32U - n)); }
}

std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> bytes) noexcept {
    std::array<std::uint32_t, 8> h = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    const std::size_t bit_len = bytes.size() * 8U;
    const std::size_t total = ((bytes.size() + 9U + 63U) / 64U) * 64U;
    std::vector<std::uint8_t> padded(total, 0U);
    std::copy(bytes.begin(), bytes.end(), padded.begin());
    padded[bytes.size()] = 0x80U;
    for (unsigned i=0;i<8U;++i) padded[total - 1U - i] = static_cast<std::uint8_t>(bit_len >> (8U*i));
    for (std::size_t block = 0; block < total; block += 64U) {
        std::array<std::uint8_t, 64> chunk{};
        std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(block), 64U, chunk.begin());
        std::array<std::uint32_t, 64> w{};
        for (unsigned i=0;i<16U;++i) w[i]=(static_cast<std::uint32_t>(chunk[i*4U])<<24U)|(static_cast<std::uint32_t>(chunk[i*4U+1U])<<16U)|(static_cast<std::uint32_t>(chunk[i*4U+2U])<<8U)|chunk[i*4U+3U];
        for (unsigned i=16U;i<64U;++i) { const auto s0=rotr(w[i-15U],7U)^rotr(w[i-15U],18U)^(w[i-15U]>>3U); const auto s1=rotr(w[i-2U],17U)^rotr(w[i-2U],19U)^(w[i-2U]>>10U); w[i]=w[i-16U]+s0+w[i-7U]+s1; }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],x=h[7];
        for (unsigned i=0;i<64U;++i) { const auto S1=rotr(e,6U)^rotr(e,11U)^rotr(e,25U); const auto ch=(e&f)^((~e)&g); const auto t1=x+S1+ch+kK[i]+w[i]; const auto S0=rotr(a,2U)^rotr(a,13U)^rotr(a,22U); const auto maj=(a&b)^(a&c)^(b&c); const auto t2=S0+maj; x=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=x;
    }
    std::array<std::uint8_t, 32> result{}; for (unsigned i=0;i<8U;++i) for (unsigned j=0;j<4U;++j) result[i*4U+j]=static_cast<std::uint8_t>(h[i]>>(24U-8U*j)); return result;
}
}
