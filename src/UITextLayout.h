#pragma once

#include "LambUI/IRenderer.h"
#include <algorithm>
#include <cmath>

namespace LambUI::TextLayout {

struct Line {
    size_t begin = 0;
    size_t end = 0;
    float width = 0.0f;
};

struct Layout {
    std::vector<Line> lines;
    float lineHeight = 16.0f;
};

inline size_t Next(const std::string& text, size_t position) {
    if (position < text.size()) ++position;
    while (position < text.size() && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) ++position;
    return position;
}

inline float Width(const std::string& text, size_t begin, size_t end,
                   const ITextMeasurer* measurer, void* font) {
    const auto part = text.substr(begin, end - begin);
    if (measurer) {
        float width = 0.0f, height = 0.0f;
        measurer->MeasureText(part, font, width, height);
        return std::isfinite(width) ? std::max(0.0f, width) : 0.0f;
    }
    return 8.0f * static_cast<float>(std::count_if(part.begin(), part.end(),
        [](unsigned char character) { return (character & 0xC0) != 0x80; }));
}

inline Layout Build(const std::string& text, const ITextMeasurer* measurer,
                    void* font, float width, bool wrap) {
    Layout result;
    if (measurer) {
        float unusedWidth = 0.0f;
        measurer->MeasureText("M", font, unusedWidth, result.lineHeight);
        if (!std::isfinite(result.lineHeight) || result.lineHeight <= 0.0f) result.lineHeight = 16.0f;
    }
    size_t paragraph = 0;
    for (;;) {
        const size_t separator = text.find_first_of("\r\n", paragraph);
        const size_t end = separator == std::string::npos ? text.size() : separator;
        size_t begin = paragraph;
        do {
            size_t finish = end;
            if (wrap) {
                size_t cursor = begin;
                size_t wordBreak = begin;
                while (cursor < end) {
                    const size_t next = Next(text, cursor);
                    if (Width(text, begin, next, measurer, font) > width) {
                        if (cursor > begin && (text[cursor] == ' ' || text[cursor] == '\t')) {
                            finish = next;
                            while (finish < end && (text[finish] == ' ' || text[finish] == '\t')) ++finish;
                        } else finish = wordBreak > begin ? wordBreak : (cursor > begin ? cursor : next);
                        break;
                    }
                    if (text[cursor] == ' ' || text[cursor] == '\t') wordBreak = next;
                    cursor = next;
                }
            }
            result.lines.push_back({begin, finish, Width(text, begin, finish, measurer, font)});
            begin = finish;
        } while (begin < end);
        if (separator == std::string::npos) break;
        paragraph = separator + 1;
        if (text[separator] == '\r' && paragraph < text.size() && text[paragraph] == '\n') ++paragraph;
    }
    return result;
}

inline size_t CursorLine(const Layout& layout, size_t cursor, bool lineEnd = false) {
    for (size_t index = 0; index + 1 < layout.lines.size(); ++index) {
        if (cursor < layout.lines[index + 1].begin || (lineEnd && cursor == layout.lines[index].end)) return index;
    }
    return layout.lines.size() - 1;
}

struct View {
    Layout layout;
    UIRect clip;
    float caretX = 0.0f;
    float caretY = 0.0f;
    float caretWidth = 0.0f;
    float caretHeight = 0.0f;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
};

inline View BuildView(const std::string& text, const ITextMeasurer* measurer,
                      void* font, const UIRect& content, bool wrap, size_t cursor,
                      bool lineEnd, float scrollX, float scrollY, bool revealCaret) {
    View view;
    view.clip = {content.x + 4.0f, content.y + 4.0f,
        std::max(0.0f, content.width - 8.0f), std::max(0.0f, content.height - 8.0f)};
    view.caretWidth = std::min(1.0f, view.clip.width);
    view.layout = Build(text, measurer, font, view.clip.width - view.caretWidth, wrap);
    const size_t index = CursorLine(view.layout, cursor, lineEnd);
    view.caretX = Width(text, view.layout.lines[index].begin, cursor, measurer, font);
    if (wrap) view.caretX = std::min(view.caretX, view.clip.width - view.caretWidth);
    view.caretY = static_cast<float>(index) * view.layout.lineHeight;
    view.caretHeight = std::min(view.clip.height, view.layout.lineHeight);
    float maximumWidth = 0.0f;
    for (const auto& line : view.layout.lines) maximumWidth = std::max(maximumWidth, line.width);
    const float maximumX = wrap ? 0.0f : std::max(0.0f, maximumWidth + view.caretWidth - view.clip.width);
    const float maximumY = std::max(0.0f, static_cast<float>(view.layout.lines.size()) * view.layout.lineHeight - view.clip.height);
    view.scrollX = std::clamp(scrollX, 0.0f, maximumX);
    view.scrollY = std::clamp(scrollY, 0.0f, maximumY);
    if (revealCaret) {
        view.scrollX = std::max(view.scrollX, view.caretX + view.caretWidth - view.clip.width);
        view.scrollX = std::min(view.scrollX, view.caretX);
        view.scrollY = std::max(view.scrollY, view.caretY + view.caretHeight - view.clip.height);
        view.scrollY = std::min(view.scrollY, view.caretY);
    }
    return view;
}

inline size_t HitPosition(const std::string& text, const Line& line, float horizontal,
                          const ITextMeasurer* measurer, void* font) {
    size_t closest = line.begin;
    float distance = std::abs(horizontal);
    for (size_t position = line.begin; position < line.end;) {
        position = Next(text, position);
        const float candidate = std::abs(Width(text, line.begin, position, measurer, font) - horizontal);
        if (candidate <= distance) {
            closest = position;
            distance = candidate;
        }
    }
    return closest;
}

} // namespace LambUI::TextLayout