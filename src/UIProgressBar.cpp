#include "LambUI/UIProgressBar.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace { constexpr const char* TAG = "UIProgressBar"; }

UIProgressBar::UIProgressBar(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    SetMouseEnabled(false);
}

void UIProgressBar::SetMinMaxValues(float minValue, float maxValue) {
    LAMBUI_LOGT(TAG, "'{}' SetMinMaxValues({}, {})", GetName(), minValue, maxValue);
    if (!std::isfinite(minValue) || !std::isfinite(maxValue)) return;
    m_minValue = std::min(minValue, maxValue);
    m_maxValue = std::max(minValue, maxValue);
    SetValue(m_value);
    MarkDirty();
}

void UIProgressBar::SetValue(float value) {
    if (!std::isfinite(value)) return;
    const float clamped = std::clamp(value, m_minValue, m_maxValue);
    if (clamped == m_value) return;
    LAMBUI_LOGT(TAG, "'{}' value {} -> {}", GetName(), m_value, clamped);
    m_value = clamped;
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnValueChanged});
}

void UIProgressBar::SetOrientation(ProgressOrientation orientation) {
    LAMBUI_LOGT(TAG, "'{}' SetOrientation({})", GetName(), ToString(orientation));
    m_orientation = orientation;
    MarkDirty();
}

void UIProgressBar::SetColors(uint32_t background, uint32_t fill) {
    LAMBUI_LOGT(TAG, "'{}' SetColors({}, {})", GetName(), background, fill);
    m_background = background;
    m_fill = fill;
    MarkDirty();
}

void UIProgressBar::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    UIRenderCommand command;
    command.x = rect.x;
    command.y = rect.y;
    command.width = std::max(0.0f, rect.width);
    command.height = std::max(0.0f, rect.height);
    command.color = m_background;
    bucket.push_back(command);
    const double range = static_cast<double>(m_maxValue) - m_minValue;
    const float fraction = range > 0.0 ? static_cast<float>((static_cast<double>(m_value) - m_minValue) / range) : 0.0f;
    if (fraction <= 0.0f) return;
    command.color = m_fill;
    if (m_orientation == ProgressOrientation::Horizontal) {
        command.width *= fraction;
    } else {
        command.y += command.height * (1.0f - fraction);
        command.height *= fraction;
    }
    bucket.push_back(command);
}

} // namespace LambUI