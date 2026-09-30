#include "LambUI/UITextWidget.h"
#include "LambUI/IRenderer.h"

namespace LambUI {

UITextWidget::UITextWidget(std::string name) : UIWidget(std::move(name)) {}

void UITextWidget::SetText(const std::string& text) {
    m_text = text;
    if (m_textMeasurer) {
        float width = 0.0f, height = 0.0f;
        m_textMeasurer->MeasureText(m_text, m_fontHandle, width, height);
        SetSize(width, height);
    }
    MarkDirty();
}

void UITextWidget::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect& rect = GetComputedRect();
    UIRenderCommand cmd;
    cmd.type = RenderCommandType::DrawString;
    cmd.x = rect.x;
    cmd.y = rect.y;
    cmd.width = rect.width;
    cmd.height = rect.height;
    cmd.color = m_color;
    cmd.fontHandle = m_fontHandle;
    cmd.text = m_text;
    bucket.push_back(cmd);
}

} // namespace LambUI
