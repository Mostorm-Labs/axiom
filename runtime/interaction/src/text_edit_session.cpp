#include "canvas/interaction/text_edit_session.hpp"

#include "canvas/interaction/session_manager.hpp"
#include "canvas/semantic/rich_text_state.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::interaction {
namespace {
using namespace runtime;
using namespace semantic;
constexpr std::size_t limit = 65536;
struct Applying final {
    bool& flag;
    explicit Applying(bool& value) : flag(value) {
        flag = true;
    }
    ~Applying() {
        flag = false;
    }
};
bool utf16(const std::string& input, std::u16string& out) {
    out.clear();
    for (std::size_t i = 0; i < input.size();) {
        const auto first = static_cast<unsigned char>(input[i++]);
        std::uint32_t cp = first;
        unsigned n = 0;
        if (first >= 0xF0) {
            cp = first & 7U;
            n = 3;
        } else if (first >= 0xE0) {
            cp = first & 15U;
            n = 2;
        } else if (first >= 0xC0) {
            cp = first & 31U;
            n = 1;
        } else if (first >= 0x80)
            return false;
        for (unsigned j = 0; j < n; ++j) {
            if (i == input.size())
                return false;
            const auto c = static_cast<unsigned char>(input[i++]);
            if ((c & 0xC0) != 0x80)
                return false;
            cp = (cp << 6U) | (c & 63U);
        }
        if ((n == 1 && cp < 128) || (n == 2 && cp < 2048) || (n == 3 && cp < 65536) ||
            cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        if (cp > 65535) {
            cp -= 65536;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10U)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 1023)));
        } else
            out.push_back(static_cast<char16_t>(cp));
        if (out.size() > limit)
            return false;
    }
    return true;
}
std::optional<std::string> utf8(const std::u16string& input) {
    if (input.size() > limit)
        return std::nullopt;
    std::string out;
    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint32_t cp = input[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (++i == input.size() || input[i] < 0xDC00 || input[i] > 0xDFFF)
                return std::nullopt;
            cp = 65536 + ((cp - 0xD800) << 10U) + (input[i] - 0xDC00);
        } else if (cp >= 0xDC00 && cp <= 0xDFFF)
            return std::nullopt;
        if (cp < 128)
            out.push_back(static_cast<char>(cp));
        else if (cp < 2048) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6U)));
            out.push_back(static_cast<char>(0x80 | (cp & 63)));
        } else if (cp < 65536) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12U)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6U) & 63)));
            out.push_back(static_cast<char>(0x80 | (cp & 63)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18U)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12U) & 63)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6U) & 63)));
            out.push_back(static_cast<char>(0x80 | (cp & 63)));
        }
    }
    return out;
}
bool boundary(const std::u16string& text, std::size_t offset) {
    return offset <= text.size() && (offset == 0 || offset == text.size() ||
                                     !(text[offset] >= 0xDC00 && text[offset] <= 0xDFFF));
}
bool validRange(const std::u16string& text, TextRange range) {
    const auto end = static_cast<std::uint64_t>(range.start) + range.length;
    return end <= text.size() && boundary(text, range.start) &&
           boundary(text, static_cast<std::size_t>(end));
}
bool project(const RichTextContent& content, std::u16string& out) {
    out.clear();
    bool first = true;
    for (const auto& paragraph : content.document.paragraphs) {
        if (!first)
            out.push_back(u'\n');
        first = false;
        for (const auto& run : paragraph.runs) {
            std::u16string part;
            if (!utf16(run.text, part))
                return false;
            out += part;
            if (out.size() > limit)
                return false;
        }
    }
    return true;
}
struct Position {
    std::size_t paragraph = 0;
    std::uint32_t scalar = 0;
};
std::optional<Position> locate(const RichTextContent& content, std::size_t offset) {
    for (std::size_t i = 0; i < content.document.paragraphs.size(); ++i) {
        std::u16string text;
        for (const auto& run : content.document.paragraphs[i].runs) {
            std::u16string part;
            if (!utf16(run.text, part))
                return std::nullopt;
            text += part;
        }
        if (offset <= text.size()) {
            if (!boundary(text, offset))
                return std::nullopt;
            std::uint32_t count = 0;
            for (std::size_t j = 0; j < offset; ++j) {
                if (text[j] >= 0xD800 && text[j] <= 0xDBFF)
                    ++j;
                ++count;
            }
            return Position{i, count};
        }
        offset -= text.size() + 1;
    }
    return std::nullopt;
}
std::uint32_t scalarCount(const Paragraph& paragraph) {
    std::uint32_t n = 0;
    for (const auto& run : paragraph.runs)
        for (const auto c : run.text)
            if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
                ++n;
    return n;
}
bool sameSession(TextSessionToken a, TextSessionToken b) {
    a.projection = b.projection = 0;
    return a == b;
}
} // namespace

TextEditSession::TextEditSession(TextEditPort& owner, InteractionSessionManager& manager)
    : owner_(owner), manager_(manager) {}
void TextEditSession::configure(std::uint64_t instance, std::uint64_t view) noexcept {
    runtimeInstance_ = instance;
    view_ = view;
}
runtime::TextReceipt TextEditSession::outcome(runtime::TextStatus status,
                                              std::uint64_t request) const {
    return {status, state_.token, request, owner_.textGeneration(), lastOperation_, false};
}
semantic::ObjectId TextEditSession::identity() {
    return owner_.allocateTextIdentity();
}

void TextEditSession::end() noexcept {
    if (state_.phase != TextPhase::kClosed)
        (void)manager_.finish(state_.token.session);
    state_.phase = TextPhase::kClosed;
    state_.finishEffect = 0;
    state_.composition = 0;
    state_.preedit.clear();
    state_.preeditSelection.reset();
}
void TextEditSession::detach() noexcept {
    end();
    if (attachment_ != std::numeric_limits<std::uint64_t>::max())
        ++attachment_;
}
void TextEditSession::refresh() {
    if (applying_ || state_.phase == TextPhase::kClosed)
        return;
    if (owner_.textClosed()) {
        detach();
        return;
    }
    if (!base_)
        return;
    const auto* current = owner_.textRecord(base_->id);
    if (!current || current->kind != ObjectKind::kRichText) {
        detach();
        return;
    }
    if (current->kind_version != base_->kind_version || current->content != base_->content) {
        state_.exitStatus = TextStatus::kRejected;
        state_.phase = TextPhase::kEditing;
        state_.composition = 0;
        state_.finishEffect = 0;
        state_.preedit.clear();
        state_.preeditSelection.reset();
        failed_.reset();
        terminal_.reset();
        if (state_.token.projection == std::numeric_limits<std::uint64_t>::max()) {
            detach();
            return;
        }
        ++state_.token.projection;
        base_ = *current;
        const auto& content = std::get<RichTextContent>(current->content);
        if (!project(content, state_.projection)) {
            detach();
            return;
        }
        state_.width = content.layout ? content.layout->wrap_width : 128;
        state_.selection = {};
        state_.replacement = {};
    }
}
runtime::TextEditorSnapshot TextEditSession::read() {
    refresh();
    return state_;
}
std::optional<runtime::TextReceipt> TextEditSession::receipt(const TextSessionToken& token,
                                                             std::uint64_t sequence) const {
    for (const auto& item : journal_)
        if (sameSession(item.first.token, token) && item.first.sequence == sequence)
            return item.second;
    return std::nullopt;
}
bool TextEditSession::acknowledge(const TextSessionToken& token, std::uint64_t through) {
    if (!sameSession(token, state_.token) || through >= state_.nextInputSequence || failed_)
        return false;
    acknowledged_ = std::max(acknowledged_, through);
    std::erase_if(journal_, [&](const auto& item) { return item.first.sequence <= acknowledged_; });
    return true;
}
runtime::TextStatus TextEditSession::finish() {
    refresh();
    if (state_.phase == TextPhase::kClosed)
        return TextStatus::kApplied;
    if (state_.phase == TextPhase::kCommitFailed)
        return TextStatus::kRejected;
    if (state_.composition != 0) {
        if (state_.finishEffect == 0) {
            if (effect_ == std::numeric_limits<std::uint64_t>::max())
                return TextStatus::kRejected;
            state_.finishEffect = ++effect_;
        }
        state_.phase = TextPhase::kFinishing;
        state_.exitStatus = TextStatus::kQueued;
        return TextStatus::kQueued;
    }
    state_.exitStatus = TextStatus::kApplied;
    end();
    return TextStatus::kApplied;
}
runtime::TextStatus TextEditSession::applyDelta(RichTextDelta delta) {
    if (state_.token.projection == std::numeric_limits<std::uint64_t>::max() || !base_ ||
        !owner_.textWritable(base_->id))
        return TextStatus::kRejected;
    delta.delta_version = 2;
    delta.expected_state_digest = richTextStateDigest(owner_.textDocument(), *base_);
    const Operation forward{owner_.allocateTextOperation(),
                            owner_.textDocument(),
                            1,
                            1,
                            EditRichTextOp{base_->id, std::move(delta)}};
    const auto prepared = owner_.prepareTextOperation(forward);
    if (prepared.disposition != PrepareDisposition::kPrepared || !prepared.plan ||
        prepared.plan->replacements.size() != 1)
        return TextStatus::kRejected;
    const auto& post = prepared.plan->replacements.front();
    const RichTextState before{base_->kind_version, std::get<RichTextContent>(base_->content)};
    const Operation inverse{owner_.allocateTextOperation(),
                            owner_.textDocument(),
                            1,
                            1,
                            EditRichTextOp{base_->id,
                                           {2,
                                            {RestoreRichTextStateStep{before}},
                                            richTextStateDigest(owner_.textDocument(), post)}}};
    std::u16string confirmed;
    if (!project(std::get<RichTextContent>(post.content), confirmed))
        return TextStatus::kRejected;
    Applying guard(applying_);
    if (!owner_.applyTextOperation(forward, inverse))
        return TextStatus::kRejected;
    lastOperation_ = forward.id.value();
    base_ = *owner_.textRecord(base_->id);
    const auto& content = std::get<RichTextContent>(base_->content);
    state_.width = content.layout ? content.layout->wrap_width : 128;
    state_.projection = std::move(confirmed);
    ++state_.token.projection;
    return TextStatus::kApplied;
}
runtime::TextStatus TextEditSession::replace(TextRange range, const std::u16string& text) {
    if (state_.token.projection == std::numeric_limits<std::uint64_t>::max() ||
        !validRange(state_.projection, range) || !utf8(text))
        return TextStatus::kRejected;
    auto projected = state_.projection;
    projected.replace(range.start, range.length, text);
    if (projected.size() > limit)
        return TextStatus::kRejected;
    if (projected == state_.projection)
        return TextStatus::kObserved;
    if (!base_) {
        if (std::none_of(
                projected.begin(), projected.end(), [](char16_t c) { return c != u'\n'; })) {
            state_.projection = projected;
            ++state_.token.projection;
            state_.selection = {static_cast<std::uint32_t>(range.start + text.size()), 0};
            return TextStatus::kObserved;
        }
        RichTextContent content;
        content.layout = RichTextLayout{state_.width};
        std::size_t start = 0;
        do {
            const auto end = projected.find(u'\n', start);
            const auto part =
                projected.substr(start, end == std::u16string::npos ? end : end - start);
            Paragraph p{identity(), {ParagraphAlignment::kLeft, 1, 0, 0}, {}};
            if (!part.empty())
                p.runs.push_back({*utf8(part), style_});
            content.document.paragraphs.push_back(std::move(p));
            if (end == std::u16string::npos)
                break;
            start = end + 1;
        } while (start <= projected.size());
        auto object = draft_;
        object.content = std::move(content);
        std::vector<std::uint8_t> order;
        for (const auto& existing : owner_.textObjects())
            if (!existing.placement.parent_id &&
                (order.empty() || OrderKey(order) < existing.placement.order_key))
                order.assign(existing.placement.order_key.bytes().begin(),
                             existing.placement.order_key.bytes().end());
        if (order.size() >= OrderKey::kMaxSize)
            return TextStatus::kRejected;
        order.push_back(128);
        object.placement.order_key = OrderKey(std::move(order));
        const Operation forward{
            owner_.allocateTextOperation(), owner_.textDocument(), 1, 1, InsertObjectsOp{{object}}};
        const Operation inverse{owner_.allocateTextOperation(),
                                owner_.textDocument(),
                                1,
                                1,
                                DeleteObjectsOp{{object.id}}};
        Applying guard(applying_);
        if (!owner_.applyTextOperation(forward, inverse))
            return TextStatus::kRejected;
        base_ = *owner_.textRecord(object.id);
        state_.projection = projected;
        state_.selection = {static_cast<std::uint32_t>(range.start + text.size()), 0};
        lastOperation_ = forward.id.value();
        ++state_.token.projection;
        return TextStatus::kApplied;
    }
    const auto& content = std::get<RichTextContent>(base_->content);
    if (content.document.paragraphs.empty()) {
        auto restored = content;
        std::size_t start = 0;
        do {
            const auto end = projected.find(u'\n', start);
            const auto part =
                projected.substr(start, end == std::u16string::npos ? end : end - start);
            Paragraph paragraph{identity(), {ParagraphAlignment::kLeft, 1, 0, 0}, {}};
            if (!part.empty())
                paragraph.runs.push_back({*utf8(part), style_});
            restored.document.paragraphs.push_back(std::move(paragraph));
            if (end == std::u16string::npos)
                break;
            start = end + 1;
        } while (start <= projected.size());
        const auto result = applyDelta(
            {2, {RestoreRichTextStateStep{{base_->kind_version, std::move(restored)}}}, {}});
        if (result == TextStatus::kApplied)
            state_.selection = {static_cast<std::uint32_t>(range.start + text.size()), 0};
        return result;
    }
    const auto first = locate(content, range.start),
               last = locate(content, static_cast<std::size_t>(range.start) + range.length);
    if (!first || !last)
        return TextStatus::kRejected;
    RichTextDelta delta;
    delta.delta_version = 2;
    auto insertionStyle = style_;
    std::uint32_t consumed = 0;
    for (const auto& run : content.document.paragraphs[first->paragraph].runs) {
        std::uint32_t count = 0;
        for (const auto c : run.text)
            if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
                ++count;
        if (first->scalar <= consumed + count) {
            insertionStyle = run.style;
            break;
        }
        consumed += count;
    }
    auto paragraph = content.document.paragraphs[first->paragraph].id;
    if (first->paragraph == last->paragraph) {
        if (range.length)
            delta.steps.push_back(
                DeleteTextStep{paragraph, first->scalar, last->scalar - first->scalar});
    } else {
        const auto tail =
            scalarCount(content.document.paragraphs[first->paragraph]) - first->scalar;
        if (tail)
            delta.steps.push_back(DeleteTextStep{paragraph, first->scalar, tail});
        if (last->scalar)
            delta.steps.push_back(
                DeleteTextStep{content.document.paragraphs[last->paragraph].id, 0, last->scalar});
        for (std::size_t i = first->paragraph + 1; i < last->paragraph; ++i) {
            const auto count = scalarCount(content.document.paragraphs[i]);
            if (count)
                delta.steps.push_back(DeleteTextStep{content.document.paragraphs[i].id, 0, count});
        }
        for (std::size_t i = first->paragraph + 1; i <= last->paragraph; ++i)
            delta.steps.push_back(MergeParagraphStep{paragraph, content.document.paragraphs[i].id});
    }
    std::size_t start = 0;
    auto scalar = first->scalar;
    do {
        const auto end = text.find(u'\n', start);
        const auto part = text.substr(start, end == std::u16string::npos ? end : end - start);
        const auto encoded = *utf8(part);
        if (!part.empty()) {
            delta.steps.push_back(InsertTextStep{paragraph, scalar, encoded, insertionStyle});
            for (const auto c : encoded)
                if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
                    ++scalar;
        }
        if (end == std::u16string::npos)
            break;
        const auto next = identity();
        delta.steps.push_back(SplitParagraphStep{paragraph, scalar, next});
        paragraph = next;
        scalar = 0;
        start = end + 1;
    } while (start <= text.size());
    const auto result = applyDelta(std::move(delta));
    if (result == TextStatus::kApplied)
        state_.selection = {static_cast<std::uint32_t>(range.start + text.size()), 0};
    return result;
}
runtime::TextReceipt TextEditSession::intent(const TextIntent& request) {
    refresh();
    lastOperation_ = {};
    if (request.kind == TextIntentKind::kCreate || request.kind == TextIntentKind::kEdit) {
        if (state_.phase != TextPhase::kClosed || !owner_.textReady() || runtimeInstance_ == 0)
            return outcome(TextStatus::kBusy, request.requestId);
        const auto* manifest = owner_.textManifest();
        if (!manifest)
            return outcome(TextStatus::kRejected, request.requestId);
        const auto font =
            std::find_if(manifest->entries.begin(), manifest->entries.end(), [](const auto& entry) {
                return entry.resource_kind == ResourceKind::kFont;
            });
        if (font == manifest->entries.end() || !validateResourceManifest(*manifest))
            return outcome(TextStatus::kRejected, request.requestId);
        if (!std::isfinite(request.viewX) || !std::isfinite(request.viewY))
            return outcome(TextStatus::kRejected, request.requestId);
        std::optional<ObjectRecord> existing;
        if (request.kind == TextIntentKind::kEdit) {
            const auto* value = owner_.textRecord(request.object);
            if (!value || value->kind != ObjectKind::kRichText || !owner_.textWritable(value->id))
                return outcome(TextStatus::kRejected, request.requestId);
            existing = *value;
        }
        if (existing)
            for (const auto& paragraph :
                 std::get<RichTextContent>(existing->content).document.paragraphs)
                for (const auto& run : paragraph.runs) {
                    if (!run.style.font_resource_id ||
                        std::none_of(manifest->entries.begin(),
                                     manifest->entries.end(),
                                     [&](const auto& entry) {
                                         return entry.resource_kind == ResourceKind::kFont &&
                                                entry.resource_id == *run.style.font_resource_id;
                                     }))
                        return outcome(TextStatus::kRejected, request.requestId);
                }
        if (attachment_ == std::numeric_limits<std::uint64_t>::max() ||
            serial_ == std::numeric_limits<std::uint64_t>::max() || !manager_.start(++serial_))
            return outcome(TextStatus::kBusy, request.requestId);
        state_ = {};
        state_.phase = TextPhase::kEditing;
        state_.token = {runtimeInstance_,
                        attachment_,
                        view_,
                        serial_,
                        serial_,
                        1,
                        owner_.textDocument().value(),
                        existing ? existing->id : identity()};
        journal_.clear();
        terminal_.reset();
        failed_.reset();
        lastComposition_ = acknowledged_ = 0;
        base_ = existing;
        style_ = {font->resource_id, 16, 400, false, false, {0, 0, 0, 1}};
        if (existing) {
            const auto& content = std::get<RichTextContent>(existing->content);
            if (!project(content, state_.projection)) {
                end();
                return outcome(TextStatus::kUnsupported, request.requestId);
            }
            state_.width = content.layout ? content.layout->wrap_width : 128;
            for (const auto& p : content.document.paragraphs)
                if (!p.runs.empty()) {
                    style_ = p.runs.front().style;
                    break;
                }
        } else {
            draft_ = {};
            draft_.id = state_.token.object;
            draft_.kind = ObjectKind::kRichText;
            draft_.kind_version = 2;
            const auto point = owner_.textContentPoint(request.viewX, request.viewY);
            draft_.transform.tx = point.first;
            draft_.transform.ty = point.second;
            state_.width = 320;
        }
        return outcome(TextStatus::kApplied, request.requestId);
    }
    if (state_.phase == TextPhase::kClosed || request.token != state_.token)
        return outcome(TextStatus::kStale, request.requestId);
    switch (request.kind) {
    case TextIntentKind::kFinish:
        return outcome(finish(), request.requestId);
    case TextIntentKind::kEscape:
        if (state_.composition) {
            state_.composition = 0;
            state_.preedit.clear();
            state_.preeditSelection.reset();
            state_.finishEffect = 0;
            state_.phase = TextPhase::kEditing;
            state_.exitStatus = TextStatus::kRejected;
            failed_.reset();
            terminal_.reset();
        } else
            end();
        return outcome(TextStatus::kObserved, request.requestId);
    case TextIntentKind::kInsertParagraph:
        if (state_.composition)
            return outcome(TextStatus::kBusy, request.requestId);
        return outcome(replace(state_.selection, u"\n"), request.requestId);
    case TextIntentKind::kSetWidth:
        if (state_.composition)
            return outcome(TextStatus::kBusy, request.requestId);
        if (!validRichTextLayoutState(2, RichTextContent{{}, RichTextLayout{request.width}}))
            return outcome(TextStatus::kRejected, request.requestId);
        if (!base_) {
            if (state_.token.projection == std::numeric_limits<std::uint64_t>::max())
                return outcome(TextStatus::kRejected, request.requestId);
            state_.width = request.width;
            ++state_.token.projection;
            return outcome(TextStatus::kObserved, request.requestId);
        }
        return outcome(
            applyDelta({2, {SetRichTextLayoutStep{2, RichTextLayout{request.width}}}, {}}),
            request.requestId);
    case TextIntentKind::kRetry:
        if (!failed_)
            return outcome(TextStatus::kRejected, request.requestId);
        {
            const auto result = replace(failed_->range, failed_->text);
            if (result == TextStatus::kApplied || result == TextStatus::kObserved) {
                failed_.reset();
                state_.composition = 0;
                state_.preedit.clear();
                state_.phase = TextPhase::kEditing;
                if (state_.finishEffect) {
                    state_.exitStatus = TextStatus::kApplied;
                    end();
                }
            }
            return outcome(result, request.requestId);
        }
    case TextIntentKind::kResync:
        return outcome(TextStatus::kObserved, request.requestId);
    default:
        return outcome(TextStatus::kRejected, request.requestId);
    }
}
runtime::TextReceipt TextEditSession::input(const TextInputEvent& event) {
    refresh();
    lastOperation_ = {};
    for (const auto& entry : journal_)
        if (sameSession(entry.first.token, event.token) && entry.first.sequence == event.sequence) {
            if (entry.first != event)
                return outcome(TextStatus::kCollision, event.sequence);
            auto receipt = entry.second;
            receipt.replay = true;
            return receipt;
        }
    if (state_.phase == TextPhase::kClosed || !sameSession(event.token, state_.token))
        return outcome(TextStatus::kStale, event.sequence);
    if (event.sequence <= acknowledged_)
        return outcome(TextStatus::kExpired, event.sequence);
    if (event.sequence == std::numeric_limits<std::uint64_t>::max())
        return outcome(TextStatus::kRejected, event.sequence);
    if (event.sequence != state_.nextInputSequence)
        return outcome(TextStatus::kOutOfOrder, event.sequence);
    if (journal_.size() >= 256)
        return outcome(TextStatus::kBackpressure, event.sequence);
    auto status = TextStatus::kRejected;
    std::optional<TextReceipt> replay;
    if (terminal_ && event.kind == TextInputKind::kCommit &&
        event.composition == terminal_->composition) {
        auto copy = event;
        copy.sequence = terminal_->sequence;
        status = copy == *terminal_ && terminalReceipt_ ? terminalReceipt_->status
                                                        : TextStatus::kCollision;
        if (copy == *terminal_)
            replay = terminalReceipt_;
    } else if (event.token.projection != state_.token.projection)
        status = TextStatus::kStale;
    else if (!validRange(state_.projection, event.range) || !utf8(event.text) ||
             (event.finishEffect && event.kind != TextInputKind::kCommit) ||
             (event.preeditSelection &&
              ((event.kind != TextInputKind::kUpdate && event.kind != TextInputKind::kCommit) ||
               !validRange(event.text, *event.preeditSelection))))
        status = TextStatus::kRejected;
    else
        switch (event.kind) {
        case TextInputKind::kBegin:
            if (state_.composition || event.composition == 0 ||
                event.composition <= lastComposition_ || state_.finishEffect || !event.text.empty())
                break;
            state_.composition = event.composition;
            lastComposition_ = event.composition;
            state_.replacement = event.range;
            state_.phase = TextPhase::kComposing;
            status = TextStatus::kObserved;
            break;
        case TextInputKind::kUpdate:
            if (event.composition != state_.composition || !state_.composition ||
                event.range != state_.replacement)
                break;
            state_.preedit = event.text;
            state_.preeditSelection = event.preeditSelection;
            status = TextStatus::kObserved;
            break;
        case TextInputKind::kCancel:
            if (event.composition != state_.composition || !state_.composition)
                break;
            state_.composition = 0;
            state_.finishEffect = 0;
            state_.preedit.clear();
            state_.preeditSelection.reset();
            state_.phase = TextPhase::kEditing;
            state_.exitStatus = TextStatus::kRejected;
            failed_.reset();
            terminal_.reset();
            status = TextStatus::kObserved;
            break;
        case TextInputKind::kCommit:
            if (event.composition != state_.composition || !state_.composition ||
                event.range != state_.replacement || event.finishEffect != state_.finishEffect)
                break;
            terminal_ = event;
            state_.preedit = event.text;
            [[fallthrough]];
        case TextInputKind::kReplace:
            if (event.kind == TextInputKind::kReplace &&
                (state_.composition || event.composition != 0 || event.finishEffect != 0))
                break;
            status = replace(event.range, event.text);
            if (status == TextStatus::kApplied || status == TextStatus::kObserved) {
                state_.composition = 0;
                state_.preedit.clear();
                state_.preeditSelection.reset();
                state_.phase = TextPhase::kEditing;
                state_.selection = {
                    static_cast<std::uint32_t>(event.range.start + event.text.size()), 0};
                if (state_.finishEffect) {
                    state_.exitStatus = TextStatus::kApplied;
                    end();
                }
            } else {
                failed_ = event;
                state_.phase = TextPhase::kCommitFailed;
                if (state_.finishEffect)
                    state_.exitStatus = TextStatus::kRejected;
            }
            break;
        case TextInputKind::kSelection:
            if (!event.composition) {
                state_.selection = event.range;
                status = TextStatus::kObserved;
            }
            break;
        default:
            break;
        }
    auto receipt = replay ? *replay : outcome(status, event.sequence);
    receipt.request = event.sequence;
    receipt.replay = replay.has_value();
    if (event.kind == TextInputKind::kCommit && terminal_ && terminal_->sequence == event.sequence)
        terminalReceipt_ = receipt;
    journal_.push_back({event, receipt});
    ++state_.nextInputSequence;
    return receipt;
}
} // namespace canvas::interaction
