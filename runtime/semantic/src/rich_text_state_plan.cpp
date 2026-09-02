#include "rich_text_state_plan.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace canvas::semantic::internal {
namespace {

struct Token { std::string text; TextStyle style; };

bool nextScalar(std::string_view s, std::size_t& i, std::string& out) {
    if (i >= s.size()) return false;
    const auto first = static_cast<unsigned char>(s[i]);
    std::size_t n = 1;
    if (first < 0x80U) n = 1;
    else if ((first & 0xe0U) == 0xc0U) n = 2;
    else if ((first & 0xf0U) == 0xe0U) n = 3;
    else if ((first & 0xf8U) == 0xf0U) n = 4;
    else return false;
    if (i + n > s.size()) return false;
    for (std::size_t j = 1; j < n; ++j) {
        if ((static_cast<unsigned char>(s[i + j]) & 0xc0U) != 0x80U) return false;
    }
    out.assign(s.substr(i, n));
    i += n;
    return true;
}

std::vector<Token> tokens(const Paragraph& paragraph) {
    std::vector<Token> result;
    for (const auto& run : paragraph.runs) {
        std::size_t i = 0;
        while (i < run.text.size()) {
            std::string scalar;
            if (!nextScalar(run.text, i, scalar)) return {};
            result.push_back(Token{std::move(scalar), run.style});
        }
    }
    return result;
}

void rebuildRuns(Paragraph& paragraph, const std::vector<Token>& values) {
    paragraph.runs.clear();
    for (const auto& token : values) {
        if (token.text.empty()) continue;
        if (!paragraph.runs.empty() && paragraph.runs.back().style == token.style) {
            paragraph.runs.back().text += token.text;
        } else {
            paragraph.runs.push_back(TextRun{token.text, token.style});
        }
    }
}

Paragraph* paragraphById(RichTextDocument& document, const ObjectId& id) {
    for (auto& paragraph : document.paragraphs) if (paragraph.id == id) return &paragraph;
    return nullptr;
}

StatefulResult invalid() { return StatefulResult{StatefulIssue::kTextStateInvalid}; }

} // namespace

StatefulResult prepareRichTextDeltaState(
    const RichTextContent& current, const RichTextDelta& delta, RichTextContent* out) {
    if (out == nullptr) return invalid();
    RichTextContent result = current;
    for (const auto& step : delta.steps) {
        const auto status = std::visit([&](const auto& value) -> StatefulResult {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, InsertTextStep>) {
                Paragraph* p = paragraphById(result.document, value.paragraph_id);
                if (!p) return invalid();
                auto vals = tokens(*p);
                const auto pos = static_cast<std::size_t>(value.scalar_offset);
                if (pos > vals.size()) return invalid();
                std::vector<Token> ins;
                std::size_t i = 0;
                while (i < value.text.size()) { std::string s; if (!nextScalar(value.text, i, s)) return invalid(); ins.push_back(Token{std::move(s), value.style}); }
                vals.insert(vals.begin() + static_cast<std::ptrdiff_t>(pos), ins.begin(), ins.end());
                rebuildRuns(*p, vals); return StatefulResult{};
            } else if constexpr (std::is_same_v<T, DeleteTextStep>) {
                Paragraph* p = paragraphById(result.document, value.paragraph_id);
                if (!p || value.scalar_count == 0U) return invalid();
                auto vals = tokens(*p); const std::size_t start = value.start_scalar, count = value.scalar_count;
                if (start > vals.size() || count > vals.size() - start) return invalid();
                vals.erase(vals.begin() + static_cast<std::ptrdiff_t>(start), vals.begin() + static_cast<std::ptrdiff_t>(start + count));
                rebuildRuns(*p, vals); return StatefulResult{};
            } else if constexpr (std::is_same_v<T, SplitParagraphStep>) {
                Paragraph* p = paragraphById(result.document, value.paragraph_id);
                if (!p || value.new_paragraph_id.isZero() || paragraphById(result.document, value.new_paragraph_id)) return invalid();
                auto vals = tokens(*p); const std::size_t pos = value.scalar_offset;
                if (pos > vals.size()) return invalid();
                Paragraph right; right.id = value.new_paragraph_id; right.style = p->style;
                std::vector<Token> left(vals.begin(), vals.begin() + static_cast<std::ptrdiff_t>(pos));
                std::vector<Token> tail(vals.begin() + static_cast<std::ptrdiff_t>(pos), vals.end());
                rebuildRuns(*p, left); rebuildRuns(right, tail);
                const auto index = static_cast<std::size_t>(p - result.document.paragraphs.data());
                result.document.paragraphs.insert(result.document.paragraphs.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(right));
                return StatefulResult{};
            } else if constexpr (std::is_same_v<T, MergeParagraphStep>) {
                if (value.first_paragraph_id == value.second_paragraph_id) return invalid();
                auto first_it = std::find_if(result.document.paragraphs.begin(), result.document.paragraphs.end(), [&](const Paragraph& p){ return p.id == value.first_paragraph_id; });
                auto second_it = std::find_if(result.document.paragraphs.begin(), result.document.paragraphs.end(), [&](const Paragraph& p){ return p.id == value.second_paragraph_id; });
                if (first_it == result.document.paragraphs.end() || second_it == result.document.paragraphs.end() || second_it != first_it + 1) return invalid();
                auto vals = tokens(*first_it); auto second_vals = tokens(*second_it); vals.insert(vals.end(), second_vals.begin(), second_vals.end());
                rebuildRuns(*first_it, vals); result.document.paragraphs.erase(second_it); return StatefulResult{};
            } else if constexpr (std::is_same_v<T, SetInlineStyleStep>) {
                Paragraph* p = paragraphById(result.document, value.paragraph_id);
                if (!p || value.scalar_count == 0U) return invalid();
                auto vals = tokens(*p); const std::size_t start = value.start_scalar, count = value.scalar_count;
                if (start > vals.size() || count > vals.size() - start) return invalid();
                for (std::size_t j = start; j < start + count; ++j) vals[j].style = value.style;
                rebuildRuns(*p, vals); return StatefulResult{};
            } else {
                Paragraph* p = paragraphById(result.document, value.paragraph_id);
                if (!p) return invalid();
                p->style = value.style; return StatefulResult{};
            }
        }, step);
        if (!status.ok()) return status;
    }
    *out = std::move(result);
    return StatefulResult{};
}

} // namespace canvas::semantic::internal
