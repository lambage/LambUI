#include "LambUI/UIButton.h"

namespace LambUI {

UIButton::UIButton(std::string name) : UIControl(std::move(name)) {}

void UIButton::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    uint32_t color = m_normalColor;
    switch (GetState()) {
        case ControlState::Hovered: color = m_hoverColor; break;
        case ControlState::Pressed: color = m_pressedColor; break;
        default: break;
    }

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
