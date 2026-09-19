#pragma once
#include "canvas/input/pointer_sample.hpp"
#include <vector>
namespace canvas::input { struct PointerSampleBatch final { std::vector<PointerSample> samples; bool terminalCancel=false; }; }
