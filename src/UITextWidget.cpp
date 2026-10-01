#include "LambUI/UITextWidget.h"
#include "LambUI/IRenderer.h"
#include "UITextLayout.h"

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
    if (m_textMeasurer && !m_wordWrap) {
        const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle, 0, false);
        float width = 0.0f;
        for (const auto& line : layout.lines) width = std::max(width, line.width);
        SetSize(width, layout.lineHeight * static_cast<float>(layout.lines.size()));
    }
    MarkDirty();
}

void UITextWidget::SetWordWrap(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetWordWrap({})", GetName(), enabled);
    m_wordWrap = enabled;
    UpdateTextSize();
}

void UITextWidget::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect rect = GetContentRect();
    const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle, rect.width, m_wordWrap);
    if (m_wordWrap) {
        UIRenderCommand clip;
        clip.type = RenderCommandType::PushScissor;
        clip.x = rect.x;
        clip.y = rect.y;
        clip.width = rect.width;
        clip.height = rect.height;
        bucket.push_back(clip);
    }
    for (size_t index = 0; index < layout.lines.size(); ++index) {
        const auto& line = layout.lines[index];
        UIRenderCommand cmd;
        cmd.type = RenderCommandType::DrawString;
        cmd.x = rect.x;
        cmd.y = rect.y + static_cast<float>(index) * layout.lineHeight;
        cmd.width = line.width;
        cmd.height = layout.lineHeight;
        cmd.color = m_color;
        cmd.fontHandle = m_fontHandle;
        cmd.text = m_text.substr(line.begin, line.end - line.begin);
        bucket.push_back(cmd);
    }
    if (m_wordWrap) {
        UIRenderCommand pop;
        pop.type = RenderCommandType::PopScissor;
        bucket.push_back(pop);
    }
}

} // namespace LambUI
