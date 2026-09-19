#pragma once

#include <cstdint>

namespace canvas::interaction {

class SemanticReadPort {
  public:
    virtual ~SemanticReadPort() = default;
    [[nodiscard]] virtual bool attached() const noexcept = 0;
};

class SceneQueryPort {
  public:
    virtual ~SceneQueryPort() = default;
    [[nodiscard]] virtual bool available() const noexcept = 0;
};

class ViewStatePort {
  public:
    virtual ~ViewStatePort() = default;
    [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;
};

struct OperationRequest final {
    std::uint64_t operationId = 0;
};

struct SubmitResult final {
    bool accepted = false;
    static SubmitResult acceptedResult() noexcept { return {true}; }
    static SubmitResult rejected() noexcept { return {false}; }
};

class OperationSubmitPort {
  public:
    virtual ~OperationSubmitPort() = default;
    [[nodiscard]] virtual SubmitResult submit(const OperationRequest&) = 0;
};

class TransientPresentationPort {
  public:
    virtual ~TransientPresentationPort() = default;
    virtual void cancel(std::uint64_t sessionId) noexcept = 0;
};

} // namespace canvas::interaction
