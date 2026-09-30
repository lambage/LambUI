#include "LambUI/UITextureWidget.h"

namespace LambUI {

namespace {
constexpr const char* TAG = "UITextureWidget";
} // namespace

UITextureWidget::UITextureWidget(std::string name) : UIWidget(std::move(name)) {}

void UITextureWidget::SetUVRect(float u0, float v0, float u1, float v1) {
    LAMBUI_LOGT(TAG, "'{}' SetUVRect({}, {}, {}, {})", GetName(), u0, v0, u1, v1);
    m_u0 = u0; m_v0 = v0; m_u1 = u1; m_v1 = v1;
    MarkDirty();
}

void UITextureWidget::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect& rect = GetComputedRect();
    UIRenderCommand cmd;
    cmd.type = RenderCommandType::DrawQuad;
    cmd.x = rect.x;
    cmd.y = rect.y;
    cmd.width = rect.width;
    cmd.height = rect.height;
    cmd.u0 = m_u0; cmd.v0 = m_v0; cmd.u1 = m_u1; cmd.v1 = m_v1;
    cmd.color = m_tint;
    cmd.textureHandle = m_textureHandle;
    bucket.push_back(cmd);
}

} // namespace LambUI
