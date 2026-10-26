#pragma once
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include <google/protobuf/repeated_ptr_field.h>
#include <memory>
namespace canvas::semantic::internal {
// Mapping uses heap/stack DTOs. Construct the repeated element before handing
// ownership to protobuf so a throwing allocation cannot leave a live count
// with an uninitialized element pointer in the locked SDK's Add() path.
template<class T> T* appendOwned(google::protobuf::RepeatedPtrField<T>* values) {
    auto value=std::make_unique<T>();
    auto* result=value.get();
    values->AddAllocated(result);
    (void)value.release();
    return result;
}
}
#endif
