#include "LambUI/UIButton.h"

namespace LambUI {

namespace { constexpr const char* TAG = "UIButton"; }

UIButton::UIButton(std::string name) : UIControl(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}

void UIButton::SetKeyboardFocusColor(uint32_t color) {
    LAMBUI_LOGT(TAG, "'{}' SetKeyboardFocusColor({})", GetName(), color);
    m_focusColor = color;
    m_hasFocusColor = true;
    MarkDirty();
}

void UIButton::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    uint32_t color = HasKeyboardFocus() && AreFocusHighlightsVisible() && m_hasFocusColor ? m_focusColor : m_normalColor;
    switch (GetState()) {
        case ControlState::Hovered: color = m_hoverColor; break;
        case ControlState::Pressed: color = m_pressedColor; break;
        default: break;
    }
    if (IsKeyboardPressed()) color = m_pressedColor;

    const UIRect& rect = GetComputedRect();
    UIRenderCommand cmd;
    cmd.type = RenderCommandType::DrawQuad;
    cmd.x = rect.x;
    cmd.y = rect.y;
    cmd.width = rect.width;
    cmd.height = rect.height;
    cmd.color = color;
    cmd.textureHandle = m_textureHandle;
    bucket.push_back(cmd);
}

} // namespace LambUI
