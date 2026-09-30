#include "LambUI/UIInputBox.h"
#include "LambUI/UITypes.h"

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

UIInputBox::UIInputBox(std::string name) : UIControl(std::move(name)) {}

void UIInputBox::OnFocusGained() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusGained", GetName());
    m_isFocused = true;
    MarkDirty();
}

void UIInputBox::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    m_isFocused = false;
    MarkDirty();
}

void UIInputBox::OnKeyEvent(uint32_t scanCode, bool isDown) {
    if (!isDown) return;
    if (scanCode == ScanCode::Backspace) Backspace();
    else if (scanCode == ScanCode::Enter) SubmitEnter();
}

void UIInputBox::AppendCharacter(char32_t codepoint) {
    if (!m_isFocused) return;
    AppendUtf8(m_text, codepoint);
    LAMBUI_LOGT(TAG, "'{}' text -> '{}'", GetName(), m_text);
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnTextChanged});
}

void UIInputBox::Backspace() {
    if (!m_isFocused || m_text.empty()) return;
    m_text.pop_back();
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
    MarkDirty();
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

    UIRenderCommand textCmd;
    textCmd.type = RenderCommandType::DrawString;
    textCmd.x = rect.x + 4.0f;
    textCmd.y = rect.y + 4.0f;
    textCmd.color = 0xFFFFFFFFu;
    textCmd.text = m_text;
    bucket.push_back(textCmd);
}

} // namespace LambUI
