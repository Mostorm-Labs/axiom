#pragma once
#include "canvas/runtime/text_input.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation_engine.hpp"

#include <deque>

namespace canvas::interaction {
class InteractionSessionManager;
class TextEditPort {
  public:
    virtual ~TextEditPort() = default;
    virtual bool textReady() const noexcept = 0;
    virtual bool textClosed() const noexcept = 0;
    virtual bool textWritable(semantic::ObjectId) const noexcept = 0;
    virtual semantic::DocumentId textDocument() const noexcept = 0;
    virtual std::uint64_t textGeneration() const noexcept = 0;
    virtual const semantic::ObjectRecord* textRecord(semantic::ObjectId) const noexcept = 0;
    virtual std::vector<semantic::ObjectRecord> textObjects() const = 0;
    virtual const semantic::ResourceManifest* textManifest() const noexcept = 0;
    virtual semantic::ObjectId allocateTextIdentity() = 0;
    virtual semantic::OperationId allocateTextOperation() = 0;
    virtual semantic::PrepareResult prepareTextOperation(const semantic::Operation&) const = 0;
    virtual bool applyTextOperation(semantic::Operation, semantic::Operation) = 0;
    virtual std::pair<float, float> textContentPoint(float, float) const noexcept = 0;
};
// Owned by InteractionRuntime, sharing its singleton session manager and the
// existing CanvasEditingRuntime typed command/history lane. Never host-owned.
class TextEditSession final {
  public:
    TextEditSession(TextEditPort&, InteractionSessionManager&);
    void configure(std::uint64_t runtimeInstance, std::uint64_t view) noexcept;
    [[nodiscard]] runtime::TextReceipt intent(const runtime::TextIntent&);
    [[nodiscard]] runtime::TextReceipt input(const runtime::TextInputEvent&);
    [[nodiscard]] runtime::TextEditorSnapshot read();
    [[nodiscard]] std::optional<runtime::TextReceipt> receipt(const runtime::TextSessionToken&,
                                                              std::uint64_t) const;
    [[nodiscard]] bool acknowledge(const runtime::TextSessionToken&, std::uint64_t);
    [[nodiscard]] runtime::TextStatus finish();
    void detach() noexcept;

  private:
    void refresh();
    void end() noexcept;
    [[nodiscard]] runtime::TextReceipt outcome(runtime::TextStatus, std::uint64_t = 0) const;
    [[nodiscard]] runtime::TextStatus replace(runtime::TextRange, const std::u16string&);
    [[nodiscard]] runtime::TextStatus applyDelta(semantic::RichTextDelta);
    [[nodiscard]] semantic::ObjectId identity();
    TextEditPort& owner_;
    InteractionSessionManager& manager_;
    runtime::TextEditorSnapshot state_{};
    std::optional<semantic::ObjectRecord> base_;
    semantic::TextStyle style_{};
    semantic::ObjectRecord draft_{};
    std::deque<std::pair<runtime::TextInputEvent, runtime::TextReceipt>> journal_;
    std::optional<runtime::TextInputEvent> terminal_, failed_;
    std::uint64_t runtimeInstance_ = 0, view_ = 0, attachment_ = 1, serial_ = 0, effect_ = 0,
                  lastComposition_ = 0, acknowledged_ = 0;
    semantic::ObjectId lastOperation_{};
    bool applying_ = false;
    std::optional<runtime::TextReceipt> terminalReceipt_;
};
} // namespace canvas::interaction
