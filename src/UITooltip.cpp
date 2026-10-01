#include "LambUI/UITooltip.h"
#include "LambUI/IRenderer.h"
#include "UITextLayout.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace { constexpr const char* TAG = "UITooltip"; }

UITooltip::UITooltip(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    SetMouseEnabled(false);
    SetVisible(false);
}

void UITooltip::SetText(std::string text, const ITextMeasurer* measurer, float maximumWidth) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    m_text = std::move(text);
    if (!std::isfinite(maximumWidth)) maximumWidth = 320.0f;
    maximumWidth = std::max(0.0f, maximumWidth);
    const auto layout = TextLayout::Build(m_text, measurer, nullptr,
        std::max(0.0f, maximumWidth - 12.0f), true);
    m_lines.clear();
    m_lineHeight = layout.lineHeight;
    float width = 0.0f;
    for (const auto& line : layout.lines) {
        m_lines.push_back(m_text.substr(line.begin, line.end - line.begin));
        width = std::max(width, line.width);
    }
    SetSize(std::min(maximumWidth, width + 12.0f),
        static_cast<float>(m_lines.size()) * m_lineHeight + 8.0f);
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
    command.color = 0xFFFFFFFFu;
    for (const auto& line : m_lines) {
        if (!line.empty()) {
            command.text = line;
            bucket.push_back(command);
        }
        command.y += m_lineHeight;
    }
    command.type = RenderCommandType::PopScissor;
    bucket.push_back(command);
}

} // namespace LambUI