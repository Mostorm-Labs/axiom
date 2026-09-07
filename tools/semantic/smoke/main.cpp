#include "smoke.pb.h"
#include <string>

int main() {
  axiom_sdk_smoke::Smoke message;
  message.set_number(736);
  message.set_text("semantic-sdk-v2");
  std::string wire;
  if (!message.SerializeToString(&wire)) return 1;
  axiom_sdk_smoke::Smoke restored;
  if (!restored.ParseFromString(wire)) return 2;
  return restored.number() == 736 && restored.text() == "semantic-sdk-v2" ? 0 : 3;
}
