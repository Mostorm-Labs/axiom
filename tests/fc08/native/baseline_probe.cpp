#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/snapshot.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

std::string fc08PublicDiscovery();

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--discover") {
    std::cout << "{\"probe\":\"FC08-baseline-v1\",\"roots\":[\"Operation\",\"ObjectRecord\",\"DocumentSnapshot\"],\"public_consumer\":"
              << fc08PublicDiscovery() << "}\n";
    return 0;
  }
  if (argc != 3) {
    std::cerr << "usage: probe <Operation|ObjectRecord|DocumentSnapshot> <independent-wire-file>\n";
    return 2;
  }
  std::ifstream file(argv[2], std::ios::binary);
  if (!file) return 3;
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
  const auto observation = canvas::semantic::SemanticCodec::observeGoldenFixture(argv[1], bytes, true);
  std::cout << "{\"root\":\"" << argv[1] << "\",\"input_bytes\":" << bytes.size()
            << ",\"accepted\":" << (observation.accepted ? "true" : "false")
            << ",\"stage\":" << static_cast<unsigned>(observation.stage)
            << ",\"category\":\"" << observation.category << "\"";
  if (std::string(argv[1]) == "Operation") {
    const auto decoded = canvas::semantic::SemanticCodec::decodeProtobufOperation(bytes);
    std::cout << ",\"normal_ingress_ok\":" << (decoded.ok() ? "true" : "false")
              << ",\"semantic_error\":" << static_cast<unsigned>(decoded.error);
  } else if (std::string(argv[1]) == "DocumentSnapshot") {
    const auto decoded = canvas::semantic::SnapshotCodec::decode(bytes);
    std::cout << ",\"normal_ingress_ok\":" << (decoded.ok() ? "true" : "false")
              << ",\"semantic_error\":" << static_cast<unsigned>(decoded.error);
  }
  std::cout << "}\n";
  return 0;
}
