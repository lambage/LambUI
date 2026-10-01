#include "LambUI/UITextWidget.h"
#include "LambUI/IRenderer.h"

namespace LambUI {

namespace {
constexpr const char* TAG = "UITextWidget";
} // namespace

UITextWidget::UITextWidget(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "'{}' constructed", GetName());
}

void UITextWidget::SetText(const std::string& text) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    m_text = text;
    UpdateTextSize();
}

void UITextWidget::SetFont(void* fontHandle) {
    LAMBUI_LOGT(TAG, "'{}' SetFont({})", GetName(), fmt::ptr(fontHandle));
    m_fontHandle = fontHandle;
    UpdateTextSize();
}

void UITextWidget::SetTextMeasurer(const ITextMeasurer* measurer) {
    LAMBUI_LOGT(TAG, "'{}' SetTextMeasurer({})", GetName(), fmt::ptr(measurer));
    m_textMeasurer = measurer;
    UpdateTextSize();
}

void UITextWidget::UpdateTextSize() {
    LAMBUI_LOGT(TAG, "'{}' UpdateTextSize", GetName());
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
