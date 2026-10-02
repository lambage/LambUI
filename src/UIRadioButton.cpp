#include "LambUI/UIRadioButton.h"
#include "LambUI/UIClamp.h"
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

UIWidget* UIRadioButton::GetFocusNeighbor(uint32_t scanCode) const {
    if (scanCode != ScanCode::Left && scanCode != ScanCode::Right && scanCode != ScanCode::Up && scanCode != ScanCode::Down) return nullptr;
    std::vector<UIRadioButton*> group;
    for (const auto& sibling : GetSiblings()) {
        auto* radio = dynamic_cast<UIRadioButton*>(sibling.get());
        if (radio && radio->GetGroup() == m_group && radio->IsVisible() && radio->IsKeyboardEnabled() && radio->CanFocus()) group.push_back(radio);
    }
    const auto current = std::find(group.begin(), group.end(), this);
    if (current == group.end()) return nullptr;
    const size_t index = static_cast<size_t>(current - group.begin());
    const bool backwards = scanCode == ScanCode::Left || scanCode == ScanCode::Up;
    return group[(index + (backwards ? group.size() - 1 : 1)) % group.size()];
}

void UIRadioButton::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (scanCode == ScanCode::Left || scanCode == ScanCode::Right || scanCode == ScanCode::Up || scanCode == ScanCode::Down) {
        if (isDown && CanFocus()) SetChecked(true);
    } else UICheckBox::OnKeyEvent(scanCode, isDown);
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
            float coverage = Clamp(1.5f - std::abs(radius - 7.5f), 0.0f, 1.0f);
            if (IsChecked()) coverage = std::max(coverage, Clamp(4.5f - radius, 0.0f, 1.0f));
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