#include "LambUI/UICheckBox.h"
#include <algorithm>

namespace LambUI {
namespace { constexpr const char* TAG = "UICheckBox"; }

UICheckBox::UICheckBox(std::string name) : UIControl(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    SetSize(160.0f, 24.0f);
}

UICheckBox::~UICheckBox() { LAMBUI_LOGT(TAG, "destroyed '{}'", GetName()); }

void UICheckBox::SetText(std::string text) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    m_text = std::move(text);
    MarkDirty();
}

bool UICheckBox::AssignChecked(bool checked) {
    if (m_checked == checked) return false;
    LAMBUI_LOGT(TAG, "'{}' checked {} -> {}", GetName(), m_checked, checked);
    m_checked = checked;
    MarkDirty();
    return true;
}

void UICheckBox::SetChecked(bool checked) {
    if (AssignChecked(checked)) FireEvent(UIEventData{UIEventType::OnValueChanged});
}

void UICheckBox::SetEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetEnabled({})", GetName(), enabled);
    m_enabled = enabled;
    m_pressedKey = 0;
    MarkDirty();
}

void UICheckBox::OnFocusGained() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusGained", GetName());
    m_focused = true;
}

void UICheckBox::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    m_focused = false;
    m_pressedKey = 0;
}

void UICheckBox::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!m_enabled || !m_focused || (scanCode != ScanCode::Space && scanCode != ScanCode::Enter)) return;
    if (isDown) {
        if (!m_pressedKey) m_pressedKey = scanCode;
    } else if (m_pressedKey == scanCode) {
        m_pressedKey = 0;
        Activate();
    }
}

void UICheckBox::Activate() {
    LAMBUI_LOGT(TAG, "'{}' Activate", GetName());
    SetChecked(!m_checked);
}

void UICheckBox::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    if (m_enabled) UIControl::OnEvent(data);
    if (data.type == UIEventType::OnMouseDown || data.type == UIEventType::OnMouseUp || data.type == UIEventType::OnClick) {
        data.handled = true;
    }
    if (m_enabled && data.type == UIEventType::OnClick && data.button == MouseButton::Left) Activate();
}

void UICheckBox::GenerateIndicator(std::vector<UIRenderCommand>& bucket, float x, float y, uint32_t color) {
    UIRenderCommand quad;
    quad.x = x;
    quad.y = y;
    quad.width = quad.height = 18.0f;
    quad.color = color;
    bucket.push_back(quad);
    quad.x += 2.0f;
    quad.y += 2.0f;
    quad.width = quad.height = 14.0f;
    quad.color = 0x20282CFFu;
    bucket.push_back(quad);
    if (!m_checked) return;
    quad.width = quad.height = 2.0f;
    quad.color = color;
    for (int step = 0; step < 10; ++step) {
        quad.x = x + 3.0f + static_cast<float>(step);
        quad.y = y + (step < 4 ? 8.0f + step : 14.0f - step);
        bucket.push_back(quad);
    }
}

void UICheckBox::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x;
    clip.y = rect.y;
    clip.width = std::max(0.0f, rect.width);
    clip.height = std::max(0.0f, rect.height);
    bucket.push_back(clip);
    const uint32_t color = !m_enabled ? 0x727A7EFFu : m_focused || GetState() == ControlState::Hovered ? 0xB9E5D8FFu : 0x67DBB3FFu;
    GenerateIndicator(bucket, rect.x + 2.0f, rect.y + std::max(0.0f, (rect.height - 18.0f) * 0.5f), color);
    UIRenderCommand label;
    label.type = RenderCommandType::DrawString;
    label.x = rect.x + 28.0f;
    label.y = rect.y + 2.0f;
    label.text = m_text;
    label.color = m_enabled ? 0xFFFFFFFFu : 0x909090FFu;
    bucket.push_back(label);
    clip.type = RenderCommandType::PopScissor;
    bucket.push_back(clip);
}
} // namespace LambUI