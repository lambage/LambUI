#include "LambUI/UISlider.h"
#include <algorithm>

namespace LambUI {

UISlider::UISlider(std::string name) : UIControl(std::move(name)) {}

void UISlider::SetMinMaxValues(float minValue, float maxValue) {
    m_minValue = minValue;
    m_maxValue = maxValue;
    SetValue(m_value);
}

void UISlider::SetValue(float value) {
    const float clamped = std::clamp(value, m_minValue, m_maxValue);
    if (clamped != m_value) {
        m_value = clamped;
        MarkDirty();
        FireEvent(UIEventData{UIEventType::OnValueChanged, 0.0f, 0.0f, MouseButton::Left, false});
    }
}

void UISlider::OnDrag(float mouseX, float mouseY) {
    const UIRect& rect = GetComputedRect();
    const float range = m_maxValue - m_minValue;
    if (range == 0.0f) return;

    float t = 0.0f;
    if (m_orientation == SliderOrientation::Horizontal) {
        t = rect.width > 0.0f ? (mouseX - rect.x) / rect.width : 0.0f;
    } else {
        t = rect.height > 0.0f ? (mouseY - rect.y) / rect.height : 0.0f;
    }
    t = std::clamp(t, 0.0f, 1.0f);
    SetValue(m_minValue + t * range);
}

void UISlider::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect& rect = GetComputedRect();

    UIRenderCommand track;
    track.type = RenderCommandType::DrawQuad;
    track.x = rect.x;
    track.y = rect.y;
    track.width = rect.width;
    track.height = rect.height;
    track.color = 0xFF404040u;
    bucket.push_back(track);

    const float range = m_maxValue - m_minValue;
    const float t = range != 0.0f ? (m_value - m_minValue) / range : 0.0f;

    UIRenderCommand thumb;
    thumb.type = RenderCommandType::DrawQuad;
    thumb.color = (GetState() == ControlState::Pressed) ? 0xFFE0E0E0u : 0xFFC0C0C0u;
    if (m_orientation == SliderOrientation::Horizontal) {
        const float thumbWidth = 10.0f;
        thumb.width = thumbWidth;
        thumb.height = rect.height;
        thumb.x = rect.x + t * (rect.width - thumbWidth);
        thumb.y = rect.y;
    } else {
        const float thumbHeight = 10.0f;
        thumb.width = rect.width;
        thumb.height = thumbHeight;
        thumb.x = rect.x;
        thumb.y = rect.y + t * (rect.height - thumbHeight);
    }
    bucket.push_back(thumb);
}

} // namespace LambUI
