#pragma once
#include "canvas/input/input_router.hpp"
namespace canvas::input { class ArcInputAdapter final { public: explicit ArcInputAdapter(InputRouter& router) noexcept:router_(router){} DispatchDisposition forward(const PointerSampleBatch& batch) { return router_.dispatch(batch); } private: InputRouter& router_; }; }
