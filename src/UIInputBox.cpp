#include "LambUI/UIInputBox.h"
#include "LambUI/IRenderer.h"
#include "LambUI/UITypes.h"
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
    MarkDirty();
}

void UIInputBox::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown || !m_isFocused) return;
    if (scanCode == ScanCode::Backspace || scanCode == ScanCode::Delete ||
        scanCode == ScanCode::Home || scanCode == ScanCode::End ||
        scanCode == ScanCode::Left || scanCode == ScanCode::Right) m_caretElapsed = 0.0f;
    if (scanCode == ScanCode::Backspace) Backspace();
    else if (scanCode == ScanCode::Enter) SubmitEnter();
    else if (scanCode == ScanCode::Home) m_cursor = 0;
    else if (scanCode == ScanCode::End) m_cursor = m_text.size();
    else if (scanCode == ScanCode::Left && m_cursor > 0) {
        do { --m_cursor; } while (m_cursor > 0 && (static_cast<unsigned char>(m_text[m_cursor]) & 0xC0) == 0x80);
    } else if (scanCode == ScanCode::Right && m_cursor < m_text.size()) {
        do { ++m_cursor; } while (m_cursor < m_text.size() && (static_cast<unsigned char>(m_text[m_cursor]) & 0xC0) == 0x80);
    } else if (scanCode == ScanCode::Delete && m_cursor < m_text.size()) {
        size_t end = m_cursor + 1;
        while (end < m_text.size() && (static_cast<unsigned char>(m_text[end]) & 0xC0) == 0x80) ++end;
        m_text.erase(m_cursor, end - m_cursor);
        FireEvent(UIEventData{UIEventType::OnTextChanged});
    }
    MarkDirty();
}

void UIInputBox::AppendCharacter(char32_t codepoint) {
    if (!m_isFocused || codepoint < 32 || codepoint == 127 || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return;
    std::string encoded;
    AppendUtf8(encoded, codepoint);
    m_text.insert(m_cursor, encoded);
    m_cursor += encoded.size();
    m_caretElapsed = 0.0f;
    LAMBUI_LOGT(TAG, "'{}' text -> '{}'", GetName(), m_text);
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnTextChanged});
}

void UIInputBox::Backspace() {
    if (!m_isFocused || m_cursor == 0) return;
    const size_t end = m_cursor;
    do { --m_cursor; } while (m_cursor > 0 && (static_cast<unsigned char>(m_text[m_cursor]) & 0xC0) == 0x80);
    m_text.erase(m_cursor, end - m_cursor);
    m_caretElapsed = 0.0f;
    LAMBUI_LOGT(TAG, "'{}' text -> '{}'", GetName(), m_text);
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnTextChanged});
}

void UIInputBox::SubmitEnter() {
    if (!m_isFocused) return;
    LAMBUI_LOGT(TAG, "'{}' SubmitEnter", GetName());
    FireEvent(UIEventData{UIEventType::OnEnterPressed});
}

void UIInputBox::SetText(const std::string& text) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    m_text = text;
    m_cursor = m_text.size();
    m_caretElapsed = 0.0f;
    MarkDirty();
}

void UIInputBox::SetFont(void* fontHandle) {
    LAMBUI_LOGT(TAG, "'{}' SetFont({})", GetName(), fmt::ptr(fontHandle));
    m_fontHandle = fontHandle;
    MarkDirty();
}

void UIInputBox::UpdateCaret(float deltaTime, const ITextMeasurer* textMeasurer) {
    m_textMeasurer = textMeasurer;
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
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

    const float contentWidth = std::max(0.0f, rect.width - 8.0f);
    const float contentHeight = std::max(0.0f, rect.height - 8.0f);
    if (contentWidth <= 0.0f || contentHeight <= 0.0f) return;
    const float caretWidth = std::min(1.0f, contentWidth);
    float cursorWidth = 0.0f;
    float textHeight = 16.0f;
    if (m_isFocused) {
        const std::string prefix = m_text.substr(0, m_cursor);
        if (m_textMeasurer) {
            float unusedWidth = 0.0f;
            float unusedHeight = 0.0f;
            m_textMeasurer->MeasureText(prefix, m_fontHandle, cursorWidth, unusedHeight);
            m_textMeasurer->MeasureText(m_text.empty() ? "M" : m_text, m_fontHandle, unusedWidth, textHeight);
        } else {
            cursorWidth = 8.0f * static_cast<float>(std::count_if(prefix.begin(), prefix.end(),
                [](unsigned char character) { return (character & 0xC0) != 0x80; }));
        }
    }
    const float scrollOffset = std::max(0.0f, cursorWidth + caretWidth - contentWidth);
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x + 4.0f;
    clip.y = rect.y + 4.0f;
    clip.width = contentWidth;
    clip.height = contentHeight;
    bucket.push_back(clip);

    UIRenderCommand textCmd;
    textCmd.type = RenderCommandType::DrawString;
    textCmd.x = rect.x + 4.0f - scrollOffset;
    textCmd.y = rect.y + 4.0f;
    textCmd.color = 0xFFFFFFFFu;
    textCmd.fontHandle = m_fontHandle;
    textCmd.text = m_text;
    bucket.push_back(textCmd);

    if (m_isFocused && m_caretElapsed < 0.5f) {
        UIRenderCommand caret;
        caret.type = RenderCommandType::DrawQuad;
        caret.x = textCmd.x + cursorWidth;
        caret.y = textCmd.y;
        caret.width = caretWidth;
        caret.height = std::min(contentHeight, std::max(1.0f, textHeight));
        caret.color = textCmd.color;
        bucket.push_back(caret);
    }
    UIRenderCommand pop;
    pop.type = RenderCommandType::PopScissor;
    bucket.push_back(pop);
}

} // namespace LambUI
