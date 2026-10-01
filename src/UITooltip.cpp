#include "LambUI/UITooltip.h"
#include "LambUI/IRenderer.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace { constexpr const char* TAG = "UITooltip"; }

UITooltip::UITooltip(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    SetMouseEnabled(false);
    SetVisible(false);
}

void UITooltip::SetText(std::string text, const ITextMeasurer* measurer) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    m_text = std::move(text);
    float width = static_cast<float>(m_text.size()) * 8.0f;
    float height = 18.0f;
    if (measurer) measurer->MeasureText(m_text, nullptr, width, height);
    if (!std::isfinite(width)) width = 0.0f;
    if (!std::isfinite(height)) height = 0.0f;
    SetSize(std::max(0.0f, width) + 12.0f, std::max(0.0f, height) + 8.0f);
}

void UITooltip::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    UIRenderCommand command;
    command.x = rect.x;
    command.y = rect.y;
    command.width = rect.width;
    command.height = rect.height;
    command.color = 0x202020FFu;
    bucket.push_back(command);
    command.type = RenderCommandType::PushScissor;
    bucket.push_back(command);
    command.type = RenderCommandType::DrawString;
    command.x += 6.0f;
    command.y += 4.0f;
    command.text = m_text;
    command.color = 0xFFFFFFFFu;
    bucket.push_back(command);
    command.type = RenderCommandType::PopScissor;
    bucket.push_back(command);
}

} // namespace LambUI