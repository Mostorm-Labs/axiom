#include "canvas/ink/multipointer_replay.hpp"

#include <cassert>

int main() {
  const canvas::input::PointerKey a{1, 1, 1};
  const canvas::input::PointerKey b{1, 2, 1};
  const canvas::input::PointerKey c{1, 3, 1};
  const canvas::input::PointerKey d{1, 4, 1};
  const std::pair<canvas::input::PointerKey, std::vector<canvas::input::PointerSample>> streams[] = {
      {a, {{1, 10, 1.0F, 1.0F, 0.5F, false, a}}},
      {b, {{1, 10, 2.0F, 2.0F, 0.5F, false, b}}},
      {c, {{1, 10, 3.0F, 3.0F, 0.5F, false, c},
           {1, 10, 3.5F, 3.5F, 0.5F, false, c}}},
      {d, {{1, 10, 4.0F, 4.0F, 0.5F, false, d}}}};
  const auto result = canvas::ink::replayMultiPointer(streams);
  assert(result.cancelled.size() == 1);
  assert(result.cancelled.front() == c);
  assert(result.committed.size() == 3);
  assert(result.committed[0].id == 1);
  assert(result.committed[1].id == 2);
  assert(result.committed[2].id == 4);
  return 0;
}
