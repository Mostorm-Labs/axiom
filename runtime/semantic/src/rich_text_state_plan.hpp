#pragma once
#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/stateful_validation.hpp"
namespace canvas::semantic::internal {
[[nodiscard]] StatefulResult prepareRichTextDeltaState(const RichTextContent&, const RichTextDelta&, RichTextContent* out);
}
