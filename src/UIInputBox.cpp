#include "LambUI/UIInputBox.h"
#include "LambUI/IRenderer.h"
#include "LambUI/UITypes.h"
#include "UITextLayout.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace {
constexpr const char* TAG = "UIInputBox";

// Minimal UTF-8 encoder; input boxes store text as UTF-8 for engine/font compatibility.
void AppendUtf8(std::string& out, char32_t codepoint) {
    if (codepoint <= 0x7F) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}
} // namespace

UIInputBox::UIInputBox(std::string name) : UIControl(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}

void UIInputBox::OnFocusGained() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusGained", GetName());
    m_isFocused = true;
    m_caretElapsed = 0.0f;
    MarkDirty();
}

void UIInputBox::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    m_isFocused = false;
    m_draggingSelection = false;
    UpdateModifiers(false, false);
    MarkDirty();
}

void UIInputBox::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown || !m_isFocused) return;
    if (m_controlDown && scanCode == ScanCode::A) SelectAll();
    else if (scanCode == ScanCode::Backspace) Backspace();
    else if (scanCode == ScanCode::Enter) SubmitEnter();
    else if (scanCode == ScanCode::Home || scanCode == ScanCode::End ||
             scanCode == ScanCode::Left || scanCode == ScanCode::Right ||
             scanCode == ScanCode::Up || scanCode == ScanCode::Down) {
        const bool extend = m_shiftDown && m_selectionEnabled;
        if (!extend && GetSelectionStart() != GetSelectionEnd() &&
            (scanCode == ScanCode::Left || scanCode == ScanCode::Right)) {
            m_cursor = scanCode == ScanCode::Left ? GetSelectionStart() : GetSelectionEnd();
            m_cursorAtLineEnd = false;
            m_preferredCaretX = -1.0f;
        } else if (m_controlDown && (scanCode == ScanCode::Home || scanCode == ScanCode::End)) {
            m_cursor = scanCode == ScanCode::Home ? 0 : m_text.size();
            m_cursorAtLineEnd = false;
            m_preferredCaretX = -1.0f;
        } else MoveCursor(scanCode);
        if (!extend) m_selectionAnchor = m_cursor;
        m_caretElapsed = 0.0f;
    }
    else if (scanCode == ScanCode::Delete && m_editingEnabled) {
        m_caretElapsed = 0.0f;
        m_preferredCaretX = -1.0f;
        m_cursorAtLineEnd = false;
        if (EraseSelection()) FireEvent(UIEventData{UIEventType::OnTextChanged});
        else if (m_cursor < m_text.size()) {
            m_text.erase(m_cursor, TextLayout::Next(m_text, m_cursor) - m_cursor);
            FireEvent(UIEventData{UIEventType::OnTextChanged});
        }
    }
    const auto view = TextLayout::BuildView(m_text, m_textMeasurer, m_fontHandle, GetContentRect(),
        m_multiline && m_wordWrap, m_cursor, m_cursorAtLineEnd, m_scrollX, m_scrollY, true);
    m_scrollX = view.scrollX;
    m_scrollY = view.scrollY;
    MarkDirty();
}

void UIInputBox::MoveCursor(uint32_t scanCode) {
    LAMBUI_LOGT(TAG, "'{}' MoveCursor({})", GetName(), scanCode);
    const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle,
        std::max(0.0f, GetContentRect().width - 9.0f), m_multiline && m_wordWrap);
    const size_t index = TextLayout::CursorLine(layout, m_cursor, m_cursorAtLineEnd);
    const auto& line = layout.lines[index];
    m_caretElapsed = 0.0f;
    if (m_multiline && (scanCode == ScanCode::Up || scanCode == ScanCode::Down)) {
        if (m_preferredCaretX < 0.0f)
            m_preferredCaretX = TextLayout::Width(m_text, line.begin, m_cursor, m_textMeasurer, m_fontHandle);
        const size_t target = scanCode == ScanCode::Up ? (index == 0 ? 0 : index - 1)
                                                     : std::min(index + 1, layout.lines.size() - 1);
        if (target == index) return;
        const auto& destination = layout.lines[target];
        m_cursor = TextLayout::HitPosition(m_text, destination, m_preferredCaretX, m_textMeasurer, m_fontHandle);
        m_cursorAtLineEnd = m_cursor == destination.end;
        return;
    }
    m_preferredCaretX = -1.0f;
    m_cursorAtLineEnd = false;
    if (scanCode == ScanCode::Home) m_cursor = m_multiline ? line.begin : 0;
    else if (scanCode == ScanCode::End) {
        m_cursor = m_multiline ? line.end : m_text.size();
        m_cursorAtLineEnd = m_multiline;
    } else if (scanCode == ScanCode::Left && m_cursor > 0) {
        do { --m_cursor; } while (m_cursor > 0 && (static_cast<unsigned char>(m_text[m_cursor]) & 0xC0) == 0x80);
    } else if (scanCode == ScanCode::Right) m_cursor = TextLayout::Next(m_text, m_cursor);
}

void UIInputBox::AppendCharacter(char32_t codepoint) {
    if (!m_isFocused || !m_editingEnabled || codepoint < 32 || codepoint == 127 || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return;
    std::string encoded;
    AppendUtf8(encoded, codepoint);
    EraseSelection();
    m_text.insert(m_cursor, encoded);
    m_cursor += encoded.size();
    m_selectionAnchor = m_cursor;
    m_caretElapsed = 0.0f;
    m_preferredCaretX = -1.0f;
    m_cursorAtLineEnd = false;
    LAMBUI_LOGT(TAG, "'{}' text -> '{}'", GetName(), m_text);
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnTextChanged});
}

void UIInputBox::Backspace() {
    if (!m_isFocused || !m_editingEnabled) return;
    m_caretElapsed = 0.0f;
    m_preferredCaretX = -1.0f;
    m_cursorAtLineEnd = false;
    if (EraseSelection()) {
        FireEvent(UIEventData{UIEventType::OnTextChanged});
        return;
    }
    if (m_cursor == 0) return;
    const size_t end = m_cursor;
    do { --m_cursor; } while (m_cursor > 0 && (static_cast<unsigned char>(m_text[m_cursor]) & 0xC0) == 0x80);
    m_text.erase(m_cursor, end - m_cursor);
    m_selectionAnchor = m_cursor;
    m_caretElapsed = 0.0f;
    LAMBUI_LOGT(TAG, "'{}' text -> '{}'", GetName(), m_text);
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnTextChanged});
}

void UIInputBox::SubmitEnter() {
    if (!m_isFocused) return;
    LAMBUI_LOGT(TAG, "'{}' SubmitEnter", GetName());
    if (m_multiline) {
        if (!m_editingEnabled) return;
        EraseSelection();
        m_text.insert(m_cursor++, 1, '\n');
        m_selectionAnchor = m_cursor;
        m_caretElapsed = 0.0f;
        m_preferredCaretX = -1.0f;
        m_cursorAtLineEnd = false;
        MarkDirty();
        FireEvent(UIEventData{UIEventType::OnTextChanged});
    } else FireEvent(UIEventData{UIEventType::OnEnterPressed});
}

bool UIInputBox::PasteText(const std::string& text) {
    LAMBUI_LOGT(TAG, "'{}' PasteText(bytes={})", GetName(), text.size());
    if (!m_isFocused || !m_editingEnabled) return false;
    std::string normalized;
    for (size_t index = 0; index < text.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(text[index]);
        if (character == '\r' || character == '\n') {
            if (character == '\r' && index + 1 < text.size() && text[index + 1] == '\n') ++index;
            normalized += m_multiline ? '\n' : ' ';
        } else if (character == '\t') normalized += ' ';
        else if (character >= 32 && character != 127) normalized += static_cast<char>(character);
    }
    if (normalized.empty()) return false;
    EraseSelection();
    m_text.insert(m_cursor, normalized);
    m_cursor += normalized.size();
    m_selectionAnchor = m_cursor;
    m_caretElapsed = 0.0f;
    m_preferredCaretX = -1.0f;
    m_cursorAtLineEnd = false;
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnTextChanged});
    return true;
}

void UIInputBox::SetText(const std::string& text) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    std::string normalized;
    for (size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (character == '\r' || character == '\n') {
            if (character == '\r' && index + 1 < text.size() && text[index + 1] == '\n') ++index;
            normalized += m_multiline ? '\n' : ' ';
        } else normalized += character;
    }
    m_text = std::move(normalized);
    m_draggingSelection = false;
    m_scrollX = 0.0f;
    m_scrollY = 0.0f;
    m_cursor = m_text.size();
    m_selectionAnchor = m_cursor;
    m_caretElapsed = 0.0f;
    m_preferredCaretX = -1.0f;
    m_cursorAtLineEnd = false;
    MarkDirty();
}

void UIInputBox::SetFont(void* fontHandle) {
    LAMBUI_LOGT(TAG, "'{}' SetFont({})", GetName(), fmt::ptr(fontHandle));
    m_fontHandle = fontHandle;
    m_preferredCaretX = -1.0f;
    MarkDirty();
}

void UIInputBox::SetMultiline(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetMultiline({})", GetName(), enabled);
    if (m_multiline == enabled) return;
    m_multiline = enabled;
    SetText(m_text);
}

void UIInputBox::SetWordWrap(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetWordWrap({})", GetName(), enabled);
    m_wordWrap = enabled;
    m_preferredCaretX = -1.0f;
    m_cursorAtLineEnd = false;
    m_caretElapsed = 0.0f;
    MarkDirty();
}

size_t UIInputBox::GetSelectionStart() const {
    return std::min(m_selectionAnchor, m_cursor);
}

size_t UIInputBox::GetSelectionEnd() const {
    return std::max(m_selectionAnchor, m_cursor);
}

std::string UIInputBox::GetSelectedText() const {
    return m_text.substr(GetSelectionStart(), GetSelectionEnd() - GetSelectionStart());
}

void UIInputBox::UpdateModifiers(bool shift, bool control) {
    if (m_shiftDown == shift && m_controlDown == control) return;
    LAMBUI_LOGT(TAG, "'{}' UpdateModifiers(shift={}, control={})", GetName(), shift, control);
    m_shiftDown = shift;
    m_controlDown = control;
}

void UIInputBox::SetEditingEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetEditingEnabled({})", GetName(), enabled);
    m_editingEnabled = enabled;
    m_caretElapsed = 0.0f;
    MarkDirty();
}

void UIInputBox::SetSelectionEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetSelectionEnabled({})", GetName(), enabled);
    m_selectionEnabled = enabled;
    if (!enabled) {
        m_draggingSelection = false;
        ClearSelection();
    }
}

void UIInputBox::SetSelection(size_t anchor, size_t cursor) {
    LAMBUI_LOGT(TAG, "'{}' SetSelection({}, {})", GetName(), anchor, cursor);
    if (!m_selectionEnabled) return;
    const auto boundary = [&](size_t position) {
        position = std::min(position, m_text.size());
        while (position > 0 && position < m_text.size() &&
               (static_cast<unsigned char>(m_text[position]) & 0xC0) == 0x80) --position;
        return position;
    };
    m_selectionAnchor = boundary(anchor);
    m_cursor = boundary(cursor);
    m_cursorAtLineEnd = false;
    m_preferredCaretX = -1.0f;
    m_caretElapsed = 0.0f;
    MarkDirty();
}

void UIInputBox::SelectAll() {
    LAMBUI_LOGT(TAG, "'{}' SelectAll", GetName());
    SetSelection(0, m_text.size());
}

void UIInputBox::ClearSelection() {
    LAMBUI_LOGT(TAG, "'{}' ClearSelection", GetName());
    m_selectionAnchor = m_cursor;
    MarkDirty();
}

bool UIInputBox::EraseSelection() {
    if (GetSelectionStart() == GetSelectionEnd()) return false;
    LAMBUI_LOGT(TAG, "'{}' EraseSelection({}, {})", GetName(), GetSelectionStart(), GetSelectionEnd());
    const size_t start = GetSelectionStart();
    m_text.erase(start, GetSelectionEnd() - start);
    m_cursor = start;
    m_selectionAnchor = start;
    m_cursorAtLineEnd = false;
    m_preferredCaretX = -1.0f;
    m_caretElapsed = 0.0f;
    MarkDirty();
    return true;
}

void UIInputBox::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    UIControl::OnEvent(data);
    if (data.button != MouseButton::Left) return;
    if (data.type == UIEventType::OnMouseDown && m_isFocused) {
        PositionCursor(data.mouseX, data.mouseY, m_shiftDown && m_selectionEnabled);
        m_draggingSelection = m_selectionEnabled;
        m_dragX = data.mouseX;
        m_dragY = data.mouseY;
        m_dragElapsed = 0.0f;
    } else if (data.type == UIEventType::OnMouseUp) {
        m_draggingSelection = false;
    }
}

void UIInputBox::OnDrag(float mouseX, float mouseY) {
    if (!m_isFocused || !m_draggingSelection || !m_selectionEnabled) return;
    LAMBUI_LOGT(TAG, "'{}' OnDrag({}, {})", GetName(), mouseX, mouseY);
    m_dragX = mouseX;
    m_dragY = mouseY;
    PositionCursor(mouseX, mouseY, true);
}

void UIInputBox::PositionCursor(float mouseX, float mouseY, bool extend) {
    if (!std::isfinite(mouseX) || !std::isfinite(mouseY)) return;
    LAMBUI_LOGT(TAG, "'{}' PositionCursor({}, {}, extend={})", GetName(), mouseX, mouseY, extend);
    const auto view = TextLayout::BuildView(m_text, m_textMeasurer, m_fontHandle, GetContentRect(),
        m_multiline && m_wordWrap, m_cursor, m_cursorAtLineEnd, m_scrollX, m_scrollY, false);
    const float row = std::floor((mouseY - view.clip.y + view.scrollY) / view.layout.lineHeight);
    const size_t index = static_cast<size_t>(boost::algorithm::clamp(row, 0.0f, static_cast<float>(view.layout.lines.size() - 1)));
    const auto& line = view.layout.lines[index];
    m_cursor = TextLayout::HitPosition(m_text, line, mouseX - view.clip.x + view.scrollX, m_textMeasurer, m_fontHandle);
    m_cursorAtLineEnd = m_cursor == line.end;
    if (!extend) m_selectionAnchor = m_cursor;
    m_preferredCaretX = -1.0f;
    m_caretElapsed = 0.0f;
    const auto revealed = TextLayout::BuildView(m_text, m_textMeasurer, m_fontHandle, GetContentRect(),
        m_multiline && m_wordWrap, m_cursor, m_cursorAtLineEnd, view.scrollX, view.scrollY, true);
    m_scrollX = revealed.scrollX;
    m_scrollY = revealed.scrollY;
    MarkDirty();
}

void UIInputBox::UpdateCaret(float deltaTime, const ITextMeasurer* textMeasurer) {
    m_textMeasurer = textMeasurer;
    if (!m_isFocused || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    if (m_draggingSelection) {
        const UIRect content = GetContentRect();
        const bool outside = m_dragX < content.x + 4.0f || m_dragX >= content.x + content.width - 4.0f ||
            m_dragY < content.y + 4.0f || m_dragY >= content.y + content.height - 4.0f;
        m_dragElapsed += deltaTime;
        if (outside && m_dragElapsed >= 0.05f) {
            m_dragElapsed = 0.0f;
            PositionCursor(m_dragX, m_dragY, true);
        }
    }
    const bool wasVisible = m_caretElapsed < 0.5f;
    m_caretElapsed = std::fmod(m_caretElapsed + std::fmod(deltaTime, 1.0f), 1.0f);
    if (wasVisible != (m_caretElapsed < 0.5f)) {
        LAMBUI_LOGT(TAG, "'{}' caret visible -> {}", GetName(), m_caretElapsed < 0.5f);
    }
}

void UIInputBox::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect& rect = GetComputedRect();

    UIRenderCommand background;
    background.type = RenderCommandType::DrawQuad;
    background.x = rect.x;
    background.y = rect.y;
    background.width = rect.width;
    background.height = rect.height;
    background.color = m_isFocused ? 0xFF303030u : 0xFF202020u;
    bucket.push_back(background);

    const auto view = TextLayout::BuildView(m_text, m_textMeasurer, m_fontHandle, GetContentRect(),
        m_multiline && m_wordWrap, m_cursor, m_cursorAtLineEnd, m_scrollX, m_scrollY, m_isFocused);
    const auto& layout = view.layout;
    const float caretWidth = view.caretWidth;
    const float scrollOffset = m_scrollX = view.scrollX;
    const float scrollY = m_scrollY = view.scrollY;
    if (view.clip.width <= 0.0f || view.clip.height <= 0.0f) return;
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = view.clip.x;
    clip.y = view.clip.y;
    clip.width = view.clip.width;
    clip.height = view.clip.height;
    bucket.push_back(clip);

    const size_t selectionStart = GetSelectionStart();
    const size_t selectionEnd = GetSelectionEnd();
    if (selectionStart != selectionEnd) {
        for (size_t index = 0; index < layout.lines.size(); ++index) {
            const auto& line = layout.lines[index];
            const size_t begin = std::max(selectionStart, line.begin);
            const size_t end = std::min(selectionEnd, line.end);
            const bool newline = index + 1 < layout.lines.size() &&
                layout.lines[index + 1].begin > line.end && selectionStart <= line.end && selectionEnd > line.end;
            if (begin >= end && !newline) continue;
            const float left = TextLayout::Width(m_text, line.begin, std::min(begin, line.end), m_textMeasurer, m_fontHandle);
            const float right = newline ? std::max(left + caretWidth, line.width + 4.0f) :
                TextLayout::Width(m_text, line.begin, end, m_textMeasurer, m_fontHandle);
            UIRenderCommand highlight;
            highlight.type = RenderCommandType::DrawQuad;
            highlight.x = clip.x + left - scrollOffset;
            highlight.y = clip.y + static_cast<float>(index) * layout.lineHeight - scrollY;
            highlight.width = std::max(0.0f, right - left);
            highlight.height = layout.lineHeight;
            highlight.color = m_isFocused ? 0x287EA8FFu : 0x445A66FFu;
            bucket.push_back(highlight);
        }
    }

    UIRenderCommand textCmd;
    textCmd.type = RenderCommandType::DrawString;
    textCmd.x = clip.x - scrollOffset;
    textCmd.color = 0xFFFFFFFFu;
    textCmd.fontHandle = m_fontHandle;
    textCmd.height = layout.lineHeight;
    for (size_t index = 0; index < layout.lines.size(); ++index) {
        const auto& line = layout.lines[index];
        textCmd.y = clip.y + static_cast<float>(index) * layout.lineHeight - scrollY;
        textCmd.width = line.width;
        textCmd.text = m_text.substr(line.begin, line.end - line.begin);
        bucket.push_back(textCmd);
    }

    if (m_isFocused && m_caretElapsed < 0.5f) {
        UIRenderCommand caret;
        caret.type = RenderCommandType::DrawQuad;
        caret.x = textCmd.x + view.caretX;
        caret.y = clip.y + view.caretY - scrollY;
        caret.width = caretWidth;
        caret.height = view.caretHeight;
        caret.color = textCmd.color;
        bucket.push_back(caret);
    }
    UIRenderCommand pop;
    pop.type = RenderCommandType::PopScissor;
    bucket.push_back(pop);
}

} // namespace LambUI
