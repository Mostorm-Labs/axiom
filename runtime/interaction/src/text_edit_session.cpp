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
bool validFormat(const TextFormatPatch& patch, const ResourceManifest* manifest) {
    if(patch.fontSize && (!std::isfinite(*patch.fontSize) || *patch.fontSize<=0 || *patch.fontSize>65536))return false;
    if(patch.color)for(const auto c:*patch.color)if(!std::isfinite(c) || c<0 || c>1)return false;
    if(patch.alignment && (*patch.alignment<1 || *patch.alignment>3))return false;
    if(patch.font) {
        if(!manifest || std::none_of(manifest->entries.begin(),manifest->entries.end(),[&](const auto& e){return e.resource_kind==ResourceKind::kFont && e.resource_id.value==*patch.font;}))return false;
    }
    return true;
}
void applyFormat(TextStyle& style, const TextFormatPatch& patch) {
        if(patch.color){const auto& c=*patch.color;style.color={c[0],c[1],c[2],c[3]};}
        if(patch.fontSize)style.font_size=*patch.fontSize;
        if(patch.font)style.font_resource_id=ResourceId{*patch.font};
        if(patch.bold)style.weight=*patch.bold?700:400;
        if(patch.italic)style.italic=*patch.italic;
        if(patch.underline)style.underline=*patch.underline;
        if(patch.strikethrough)style.strikethrough=*patch.strikethrough;
}
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
const semantic::RichTextContent* TextEditSession::richContent(
    const semantic::ObjectRecord& record) const noexcept {
    if (record.kind == semantic::ObjectKind::kRichText)
        return std::get_if<semantic::RichTextContent>(&record.content);
    if (shapeLabelTarget(record)) {
        const auto* shape = std::get_if<semantic::ShapeContent>(&record.content);
        if (shape == nullptr) return nullptr;
        if (shape->label) return &*shape->label;
        static const semantic::RichTextContent empty{};
        return &empty;
    }
    return nullptr;
}
bool TextEditSession::shapeLabelTarget(const semantic::ObjectRecord& record) const noexcept {
    return record.kind == semantic::ObjectKind::kShape &&
           (record.kind_version == 1U || record.kind_version == 2U || record.kind_version == 3U) &&
           std::holds_alternative<semantic::ShapeContent>(record.content) &&
           (record.kind_version != 3U ||
            std::get<semantic::ShapeContent>(record.content).label.has_value());
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
    if (!current || richContent(*current) == nullptr) {
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
        const auto& content = *richContent(*current);
        if (!project(content, state_.projection)) {
            detach();
            return;
        }
        state_.width = content.layout ? content.layout->wrap_width :
            shapeLabelTarget(*current) ? std::get<ShapeContent>(current->content).width : 128;
        typingStyle_=false;
        state_.selection = {};
        state_.replacement = {};
    }
}
runtime::TextEditorSnapshot TextEditSession::read() {
    refresh();
    auto out=state_;
    auto selectedStyle=style_; auto paragraph=paragraphStyle_;
    if(base_ && !typingStyle_) {
        const auto& content=*richContent(*base_);
        if(const auto pos=locate(content,state_.selection.start)) {
            const auto& p=content.document.paragraphs[pos->paragraph];paragraph=p.style;
            std::uint32_t offset=0;
            for(const auto& r:p.runs) {
                Paragraph one;one.runs={r};const auto count=scalarCount(one);
                if(pos->scalar<offset+count || (!state_.selection.length && pos->scalar==offset+count)){selectedStyle=r.style;break;}offset+=count;
            }
        }
    }
    out.format.color=std::array<float,4>{selectedStyle.color.r,selectedStyle.color.g,selectedStyle.color.b,selectedStyle.color.a};
    out.format.fontSize=selectedStyle.font_size;
    if(selectedStyle.font_resource_id)out.format.font=selectedStyle.font_resource_id->value;
    out.format.bold=selectedStyle.weight>=700;out.format.italic=selectedStyle.italic;
    out.format.underline=selectedStyle.underline;out.format.strikethrough=selectedStyle.strikethrough;
    out.format.alignment=static_cast<std::uint32_t>(paragraph.alignment);out.format.bulleted=paragraph.bulleted;
    // Non-empty selections report an effect when any intersecting character
    // (or paragraph for bullets) enables it. Endpoints are end-exclusive.
    if (base_ && state_.selection.length) {
        const auto& content = *richContent(*base_);
        const auto first = locate(content, state_.selection.start);
        const auto last = locate(content, static_cast<std::size_t>(state_.selection.start) +
                                          state_.selection.length);
        if (first && last) {
            bool bold = false, italic = false, underline = false, strike = false, bullet = false;
            for (auto index = first->paragraph; index <= last->paragraph; ++index) {
                if (index > first->paragraph && index == last->paragraph && last->scalar == 0)
                    break;
                const auto& p = content.document.paragraphs[index];
                bullet = bullet || p.style.bulleted;
                const auto begin = index == first->paragraph ? first->scalar : 0U;
                const auto end = index == last->paragraph ? last->scalar : scalarCount(p);
                std::uint32_t offset = 0;
                for (const auto& run : p.runs) {
                    Paragraph one;
                    one.runs = {run};
                    const auto next = offset + scalarCount(one);
                    if (offset < end && next > begin) {
                        bold = bold || run.style.weight >= 700;
                        italic = italic || run.style.italic;
                        underline = underline || run.style.underline;
                        strike = strike || run.style.strikethrough;
                    }
                    offset = next;
                }
            }
            out.format.bold = bold;
            out.format.italic = italic;
            out.format.underline = underline;
            out.format.strikethrough = strike;
            out.format.bulleted = bullet;
        }
    }

    const auto formatTarget = state_.phase == TextPhase::kClosed ? owner_.textSelectedObject() :
        !state_.selection.length ? state_.token.object : ObjectId{};
    if (const auto* record = owner_.textRecord(formatTarget);
        record && shapeLabelTarget(*record) && !state_.composition) {
        out.formatObject = formatTarget;
        if (const auto* label = richContent(*record); label && !label->document.paragraphs.empty()) {
            const auto& first = label->document.paragraphs.front();
            if (!first.runs.empty()) {
                const auto& style = first.runs.front().style;
                out.format.font = style.font_resource_id ? std::optional<ObjectId>{style.font_resource_id->value} : std::nullopt;
                out.format.fontSize = style.font_size;
                out.format.color = std::array<float, 4>{style.color.r, style.color.g, style.color.b, style.color.a};
            }
            out.format.alignment = static_cast<std::uint32_t>(first.style.alignment);
            bool bold=false, italic=false, underline=false, strike=false, bullet=false;
            for (const auto& p : label->document.paragraphs) {
                bullet = bullet || p.style.bulleted;
                for (const auto& r : p.runs) {
                    bold = bold || r.style.weight >= 700; italic = italic || r.style.italic;
                    underline = underline || r.style.underline; strike = strike || r.style.strikethrough;
                }
            }
            out.format.bold=bold; out.format.italic=italic; out.format.underline=underline;
            out.format.strikethrough=strike; out.format.bulleted=bullet;
        }
    }

    if(const auto* manifest=owner_.textManifest())for(const auto& f:manifest->entries) {
        if(f.resource_kind!=ResourceKind::kFont)continue;
        std::string name=owner_.textFontName(f.resource_id.value);
        if(name.empty())name="Font "+f.content_hash.substr(7,8);
        out.fonts.push_back({f.resource_id.value,std::move(name)});
    }
    return out;
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
runtime::TextStatus TextEditSession::finish(bool keepEditor) {
    refresh();
    if (state_.phase == TextPhase::kClosed)
        return TextStatus::kApplied;
    if (state_.phase == TextPhase::kCommitFailed)
        return TextStatus::kRejected;
    if (state_.composition != 0) {
        if(!keepEditor)finishKeepsEditor_=false;
        if (state_.finishEffect == 0) {
            finishKeepsEditor_=keepEditor;
            if (effect_ == std::numeric_limits<std::uint64_t>::max())
                return TextStatus::kRejected;
            state_.finishEffect = ++effect_;
        }
        state_.phase = TextPhase::kFinishing;
        state_.exitStatus = TextStatus::kQueued;
        return TextStatus::kQueued;
    }
    state_.exitStatus = TextStatus::kApplied;
    if(!keepEditor)end();
    return TextStatus::kApplied;
}
runtime::TextStatus TextEditSession::applyObjectDelta(const ObjectRecord& base, RichTextDelta delta) {
    if (!owner_.textReady() || !owner_.textWritable(base.id))
        return TextStatus::kRejected;
    bool v3=base.kind_version==3;
    for(const auto& step:delta.steps)if(const auto* restore=std::get_if<RestoreRichTextStateStep>(&step))v3=v3 || restore->state.kind_version==3;
    delta.delta_version = v3?3:2;
    const auto deltaVersion=delta.delta_version;
    delta.expected_state_digest = richTextStateDigest(owner_.textDocument(), base);
    const Operation forward{owner_.allocateTextOperation(),
                            owner_.textDocument(),
                            1,
                            1,
                            EditRichTextOp{base.id, std::move(delta)}};
    const auto prepared = owner_.prepareTextOperation(forward);
    if (prepared.disposition != PrepareDisposition::kPrepared || !prepared.plan ||
        prepared.plan->replacements.size() != 1)
        return TextStatus::kRejected;
    const auto& post = prepared.plan->replacements.front();
    const auto* beforeContent = richContent(base);
    if (beforeContent == nullptr) return TextStatus::kRejected;
    RichTextState before{base.kind_version, *beforeContent};
    // An empty legacy Shape state denotes absence of the embedded label.
    // Triangle /2 still uses the existing fixed-width state wire contract.
    if (shapeLabelTarget(base) && base.kind_version == 2U)
        before.content.layout = RichTextLayout{std::get<ShapeContent>(base.content).width};
    const Operation inverse{owner_.allocateTextOperation(),
                            owner_.textDocument(),
                            1,
                            1,
                            EditRichTextOp{base.id,
                                           {deltaVersion,
                                            {RestoreRichTextStateStep{before}},
                                            richTextStateDigest(owner_.textDocument(), post)}}};
    std::u16string confirmed;
    const auto* postContent = richContent(post);
    if (postContent == nullptr || !project(*postContent, confirmed))
        return TextStatus::kRejected;
    Applying guard(applying_);
    if (!owner_.applyTextOperation(forward, inverse))
        return TextStatus::kRejected;
    lastOperation_ = forward.id.value();
    return TextStatus::kApplied;
}
runtime::TextStatus TextEditSession::applyDelta(RichTextDelta delta) {
    if (!base_ || state_.token.projection == std::numeric_limits<std::uint64_t>::max())
        return TextStatus::kRejected;
    const auto status = applyObjectDelta(*base_, std::move(delta));
    if (status != TextStatus::kApplied) return status;
    base_ = *owner_.textRecord(base_->id);
    const auto& content = *richContent(*base_);
    state_.width = content.layout ? content.layout->wrap_width : 128;
    (void)project(content, state_.projection);
    ++state_.token.projection;
    return status;
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
            Paragraph p{identity(), paragraphStyle_, {}};
            if (!part.empty())
                p.runs.push_back({*utf8(part), style_});
            content.document.paragraphs.push_back(std::move(p));
            if (end == std::u16string::npos)
                break;
            start = end + 1;
        } while (start <= projected.size());
        auto object = draft_;
        object.content = std::move(content);
        if(style_.strikethrough || paragraphStyle_.bulleted)object.kind_version=3;
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
    const auto& content = *richContent(*base_);
    if (content.document.paragraphs.empty()) {
        if (shapeLabelTarget(*base_) && base_->kind_version != 3U &&
            std::none_of(projected.begin(), projected.end(), [](char16_t c) { return c != u'\n'; })) {
            state_.projection = projected;
            ++state_.token.projection;
            state_.selection = {static_cast<std::uint32_t>(range.start + text.size()), 0};
            return TextStatus::kObserved;
        }
        auto restored = content;
        if (shapeLabelTarget(*base_)) restored.layout = RichTextLayout{state_.width};
        std::size_t start = 0;
        do {
            const auto end = projected.find(u'\n', start);
            const auto part =
                projected.substr(start, end == std::u16string::npos ? end : end - start);
            Paragraph paragraph{identity(), paragraphStyle_, {}};
            if (!part.empty())
                paragraph.runs.push_back({*utf8(part), style_});
            restored.document.paragraphs.push_back(std::move(paragraph));
            if (end == std::u16string::npos)
                break;
            start = end + 1;
        } while (start <= projected.size());
        const auto version = shapeLabelTarget(*base_) ? 3U : base_->kind_version;
        const auto result = applyDelta(
            {version >= 3U ? 3U : 2U,
             {RestoreRichTextStateStep{{version, std::move(restored)}}}, {}});
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
        if(typingStyle_)break;
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
runtime::TextStatus TextEditSession::format(const TextFormatPatch& patch) {
    if(state_.composition || state_.phase!=TextPhase::kEditing)return TextStatus::kBusy;
    if(!owner_.textReady() || (base_ && !owner_.textWritable(base_->id)))return TextStatus::kRejected;
    if (!validFormat(patch, owner_.textManifest())) return TextStatus::kRejected;
    const auto apply = [&](TextStyle& style) { applyFormat(style, patch); };
    auto newStyle=style_;apply(newStyle);
    auto newParagraph=paragraphStyle_;
    if(patch.alignment)newParagraph.alignment=static_cast<ParagraphAlignment>(*patch.alignment);
    if(patch.bulleted)newParagraph.bulleted=*patch.bulleted;
    if(!base_ || (shapeLabelTarget(*base_) && base_->kind_version != 3U)){
        style_=newStyle;paragraphStyle_=newParagraph;typingStyle_=true;++state_.token.projection;return TextStatus::kObserved;
    }
    const auto* currentContent = richContent(*base_);
    if (currentContent == nullptr) return TextStatus::kRejected;
    auto content=*currentContent;
    const auto first=locate(content,state_.selection.start),last=locate(content,static_cast<std::size_t>(state_.selection.start)+state_.selection.length);
    if(!first || !last)return TextStatus::kRejected;
    for(std::size_t index=first->paragraph;index<=last->paragraph;++index) {
        // An endpoint at the start of the next paragraph does not select it.
        if(state_.selection.length && index>first->paragraph && index==last->paragraph && last->scalar==0)break;
        auto& p=content.document.paragraphs[index];
        if(patch.alignment)p.style.alignment=static_cast<ParagraphAlignment>(*patch.alignment);
        if(patch.bulleted)p.style.bulleted=*patch.bulleted;
        if(!state_.selection.length)continue;
        const auto begin=index==first->paragraph?first->scalar:0;
        const auto end=index==last->paragraph?last->scalar:scalarCount(p);
        std::vector<TextRun> runs;std::uint32_t scalar=0;
        for(const auto& r:p.runs) {
            std::u16string text;if(!utf16(r.text,text))return TextStatus::kRejected;
            for(std::size_t offset=0;offset<text.size();++scalar) {
                auto next=offset+1;if(text[offset]>=0xD800 && text[offset]<=0xDBFF)++next;
                auto style=r.style;if(scalar>=begin && scalar<end)apply(style);
                const auto encoded=utf8(text.substr(offset,next-offset));if(!encoded)return TextStatus::kRejected;
                if(!runs.empty() && runs.back().style==style)runs.back().text+=*encoded;else runs.push_back({*encoded,style});
                offset=next;
            }
            if(r.text.empty())runs.push_back(r);
        }
        p.runs=std::move(runs);
    }
    auto version=base_->kind_version;
    if(patch.strikethrough.value_or(false) || patch.bulleted.value_or(false))version=3;
    if(version==3 && !content.layout)content.layout=RichTextLayout{state_.width};
    const auto* baseContent = richContent(*base_);
    if (baseContent == nullptr) return TextStatus::kRejected;
    const auto unchanged=content==*baseContent && version==base_->kind_version;
    const auto status=unchanged?TextStatus::kObserved:applyDelta({3,{RestoreRichTextStateStep{{version,std::move(content)}}},{}});
    if(status==TextStatus::kApplied || status==TextStatus::kObserved) {
        style_=newStyle;paragraphStyle_=newParagraph;typingStyle_=!state_.selection.length;
    }
    return status;
}
runtime::TextStatus TextEditSession::formatObject(ObjectId id, const TextFormatPatch& patch) {
    const bool editing = state_.phase != TextPhase::kClosed;
    if (editing && (state_.composition || state_.phase != TextPhase::kEditing)) return TextStatus::kBusy;
    if (editing && state_.token.object != id) return TextStatus::kBusy;
    if (!editing && owner_.textSelectedObject() != id) return TextStatus::kStale;
    if (!owner_.textReady() || !owner_.textWritable(id) || !validFormat(patch, owner_.textManifest()))
        return TextStatus::kRejected;
    if (editing && state_.token.projection == std::numeric_limits<std::uint64_t>::max()) return TextStatus::kRejected;
    const auto* current = owner_.textRecord(id);
    if (!current || !shapeLabelTarget(*current)) return TextStatus::kRejected;
    const auto before = *current;
    const auto* original = richContent(before);
    if (!original) return TextStatus::kRejected;
    auto content = *original;
    if (content.document.paragraphs.empty()) {
        const auto* manifest = owner_.textManifest();
        if (!manifest) return TextStatus::kRejected;
        const auto font = std::find_if(manifest->entries.begin(), manifest->entries.end(),
            [](const auto& entry) { return entry.resource_kind == ResourceKind::kFont; });
        if (font == manifest->entries.end()) return TextStatus::kRejected;
        TextStyle initial{font->resource_id, 16, 400, false, false, {0, 0, 0, 1}};
        content.document.paragraphs.push_back({identity(), {ParagraphAlignment::kCenter, 1, 0, 0}, {{"", initial}}});
    }
    for (auto& paragraph : content.document.paragraphs) {
        if (patch.alignment) paragraph.style.alignment = static_cast<ParagraphAlignment>(*patch.alignment);
        if (patch.bulleted) paragraph.style.bulleted = *patch.bulleted;
        for (auto& run : paragraph.runs) applyFormat(run.style, patch);
    }
    if (!content.layout) content.layout = RichTextLayout{std::get<ShapeContent>(before.content).width};
    if (before.kind_version == 3 && content == *original) return TextStatus::kObserved;
    const auto status = applyObjectDelta(before, {3, {RestoreRichTextStateStep{{3, std::move(content)}}}, {}});
    if (status == TextStatus::kApplied && editing) {
        base_ = *owner_.textRecord(id);
        ++state_.token.projection;
        applyFormat(style_, patch);
        if (patch.alignment) paragraphStyle_.alignment = static_cast<ParagraphAlignment>(*patch.alignment);
        if (patch.bulleted) paragraphStyle_.bulleted = *patch.bulleted;
    }
    return status;
}
runtime::TextReceipt TextEditSession::intent(const TextIntent& request) {
    refresh();
    lastOperation_ = {};
    if (request.kind == TextIntentKind::kFormat && !request.object.isZero()) {
        if (state_.phase != TextPhase::kClosed && request.token != state_.token)
            return outcome(TextStatus::kStale, request.requestId);
        return outcome(formatObject(request.object, request.format), request.requestId);
    }
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
            if (!value || richContent(*value) == nullptr || !owner_.textWritable(value->id))
                return outcome(TextStatus::kRejected, request.requestId);
            existing = *value;
        }
        if (existing)
            for (const auto& paragraph :
                 richContent(*existing)->document.paragraphs)
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
        typingStyle_ = false;
        paragraphStyle_ = {existing && shapeLabelTarget(*existing) ? ParagraphAlignment::kCenter
                                                                   : ParagraphAlignment::kLeft, 1, 0, 0};
        style_ = {font->resource_id, 16, 400, false, false, {0, 0, 0, 1}};
        if (existing) {
            const auto& content = *richContent(*existing);
            if (!project(content, state_.projection)) {
                end();
                return outcome(TextStatus::kUnsupported, request.requestId);
            }
            state_.width = content.layout ? content.layout->wrap_width :
                shapeLabelTarget(*existing) ? std::get<ShapeContent>(existing->content).width : 128;
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
    case TextIntentKind::kFormat:return outcome(format(request.format),request.requestId);
    case TextIntentKind::kCompleteComposition:return outcome(finish(true),request.requestId);
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
        if (!base_ || (shapeLabelTarget(*base_) && base_->kind_version != 3U)) {
            if (state_.token.projection == std::numeric_limits<std::uint64_t>::max())
                return outcome(TextStatus::kRejected, request.requestId);
            state_.width = request.width;
            ++state_.token.projection;
            return outcome(TextStatus::kObserved, request.requestId);
        }
        return outcome(
            applyDelta({2, {SetRichTextLayoutStep{base_->kind_version==3?3U:2U, RichTextLayout{request.width}}}, {}}),
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
                    if(finishKeepsEditor_){state_.finishEffect=0;state_.phase=TextPhase::kEditing;}else end();
                }
            }
            return outcome(result, request.requestId);
        }
    case TextIntentKind::kSelectAll:
        if(state_.composition)return outcome(TextStatus::kBusy,request.requestId);
        typingStyle_=false;state_.selection={0,static_cast<std::uint32_t>(state_.projection.size())};return outcome(TextStatus::kObserved,request.requestId);
    case TextIntentKind::kBackspace: case TextIntentKind::kDelete: case TextIntentKind::kLeft: case TextIntentKind::kRight: {
        if(state_.composition)return outcome(TextStatus::kBusy,request.requestId);
        auto range=state_.selection;auto breaks=owner_.textGraphemes(state_.projection);
        if(breaks.empty())return outcome(TextStatus::kUnsupported,request.requestId);
        if(request.kind==TextIntentKind::kLeft || request.kind==TextIntentKind::kRight){
            auto pos=range.start;
            if(request.kind==TextIntentKind::kLeft){auto it=std::lower_bound(breaks.begin(),breaks.end(),pos);if(!range.length && it!=breaks.begin())pos=*std::prev(it);}
            else{pos+=range.length;auto it=std::upper_bound(breaks.begin(),breaks.end(),pos);if(!range.length && it!=breaks.end())pos=*it;}
            state_.selection={pos,0};return outcome(TextStatus::kObserved,request.requestId);
        }
        if(!range.length){if(request.kind==TextIntentKind::kBackspace){auto it=std::lower_bound(breaks.begin(),breaks.end(),range.start);if(it==breaks.begin())return outcome(TextStatus::kObserved,request.requestId);range.start=*std::prev(it);range.length=state_.selection.start-range.start;}
            else{auto it=std::upper_bound(breaks.begin(),breaks.end(),range.start);if(it==breaks.end())return outcome(TextStatus::kObserved,request.requestId);range.length=*it-range.start;}}
        return outcome(replace(range,u""),request.requestId);
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
             (event.finishEffect && event.kind != TextInputKind::kCommit && event.kind != TextInputKind::kFinishFailed) ||
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
                    if(finishKeepsEditor_){state_.finishEffect=0;state_.phase=TextPhase::kEditing;}else end();
                }
            } else {
                failed_ = event;
                state_.phase = TextPhase::kCommitFailed;
                if (state_.finishEffect)
                    state_.exitStatus = TextStatus::kRejected;
            }
            break;
        case TextInputKind::kFinishFailed:
            if(!state_.composition || event.finishEffect!=state_.finishEffect || !state_.finishEffect)break;
            state_.finishEffect=0;state_.exitStatus=TextStatus::kRejected;state_.phase=TextPhase::kComposing;status=TextStatus::kRejected;break;
        case TextInputKind::kSelection:
            if (!event.composition) {
                state_.selection = event.range;
                typingStyle_=false;
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

std::optional<semantic::ObjectRecord> TextEditSession::presentation() {
    refresh();if(state_.phase==TextPhase::kClosed)return std::nullopt;
    auto object=base_?*base_:draft_;
    const auto* original=base_?richContent(*base_):nullptr;
    if(original && !state_.composition && !original->document.paragraphs.empty()){
        // Empty paragraphs still need the admitted font for a real caret metric.
        auto* content = object.kind == ObjectKind::kShape
            ? &*std::get<ShapeContent>(object.content).label
            : std::get_if<RichTextContent>(&object.content);
        if (!content) return std::nullopt;
        for(auto& p:content->document.paragraphs)if(p.runs.empty())p.runs.push_back({"",style_});
        return object;
    }
    std::u16string visible=state_.projection;std::vector<TextStyle> styles;
    if(original)for(const auto& p:original->document.paragraphs){for(const auto& r:p.runs){std::u16string part;if(!utf16(r.text,part))return std::nullopt;styles.insert(styles.end(),part.size(),r.style);}styles.push_back(style_);}
    styles.resize(visible.size(),style_);
    if(state_.composition){visible.replace(state_.replacement.start,state_.replacement.length,state_.preedit);auto it=styles.begin()+state_.replacement.start;it=styles.erase(it,it+state_.replacement.length);styles.insert(it,state_.preedit.size(),style_);}
    RichTextContent content;content.layout=original && original->layout?original->layout:std::optional<RichTextLayout>{RichTextLayout{state_.width}};
    std::size_t start=0,index=0;
    do {const auto end=visible.find(u'\n',start),stop=end==std::u16string::npos?visible.size():end;
        Paragraph p;auto transient=object.id;transient.bytes[12]^=0x80;transient.bytes[10]=static_cast<std::uint8_t>(index);transient.bytes[11]=static_cast<std::uint8_t>(index>>8U);
        p.id=original && index<original->document.paragraphs.size()?original->document.paragraphs[index].id:transient;
        p.style=original && index<original->document.paragraphs.size()?original->document.paragraphs[index].style:paragraphStyle_;
        for(auto offset=start;offset<stop;){auto next=offset+1;while(next<stop && styles[next]==styles[offset])++next;
            if(next<stop && visible[next]>=0xDC00 && visible[next]<=0xDFFF)++next;
            const auto value=utf8(visible.substr(offset,next-offset));if(!value)return std::nullopt;p.runs.push_back({*value,styles[offset]});offset=next;}
        if(p.runs.empty())p.runs.push_back({"",style_});content.document.paragraphs.push_back(std::move(p));++index;
        if(end==std::u16string::npos)break;start=end+1;
    }while(start<=visible.size());
    if (shapeLabelTarget(object)) {
        object.kind_version = 3U;
        std::get<semantic::ShapeContent>(object.content).label = std::move(content);
    } else
        object.content=std::move(content);
    return object;
}
} // namespace canvas::interaction
