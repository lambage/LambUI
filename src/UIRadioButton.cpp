#include "LambUI/UIRadioButton.h"
#include <algorithm>
#include <cmath>

namespace LambUI {
namespace { constexpr const char* TAG = "UIRadioButton"; }

UIRadioButton::UIRadioButton(std::string name) : UICheckBox(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}
UIRadioButton::~UIRadioButton() { LAMBUI_LOGT(TAG, "destroyed '{}'", GetName()); }

void UIRadioButton::SetGroup(std::string group) {
    LAMBUI_LOGT(TAG, "'{}' SetGroup('{}')", GetName(), group);
    m_group = std::move(group);
    if (IsChecked()) SetChecked(true);
}

void UIRadioButton::SetChecked(bool checked) {
    LAMBUI_LOGT(TAG, "'{}' SetChecked({})", GetName(), checked);
    std::vector<UIRadioButton*> changed;
    if (checked) {
        for (const auto& sibling : GetSiblings()) {
            auto* radio = dynamic_cast<UIRadioButton*>(sibling.get());
            if (radio && radio != this && radio->m_group == m_group && radio->AssignChecked(false)) changed.push_back(radio);
        }
    }
    if (AssignChecked(checked)) changed.push_back(this);
    for (auto* radio : changed) radio->FireEvent(UIEventData{UIEventType::OnValueChanged});
}

void UIRadioButton::Activate() {
    LAMBUI_LOGT(TAG, "'{}' Activate", GetName());
    SetChecked(true);
}

void UIRadioButton::GenerateIndicator(std::vector<UIRenderCommand>& bucket, float x, float y, uint32_t color) {
    for (int row = 0; row < 18; ++row) {
        for (int column = 0; column < 18; ++column) {
            const float deltaX = static_cast<float>(column) - 8.5f;
            const float deltaY = static_cast<float>(row) - 8.5f;
            const float radius = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            float coverage = std::clamp(1.5f - std::abs(radius - 7.5f), 0.0f, 1.0f);
            if (IsChecked()) coverage = std::max(coverage, std::clamp(4.5f - radius, 0.0f, 1.0f));
            if (coverage <= 0.0f) continue;
            UIRenderCommand quad;
            quad.x = x + static_cast<float>(column);
            quad.y = y + static_cast<float>(row);
            quad.width = quad.height = 1.0f;
            quad.color = (color & 0xFFFFFF00u) | static_cast<uint32_t>(coverage * static_cast<float>(color & 255u));
            bucket.push_back(quad);
        }
    }
}
} // namespace LambUI