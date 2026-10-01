#include "LambUI/UIControl.h"

namespace LambUI {

namespace {
constexpr const char* TAG = "UIControl";
} // namespace

UIControl::UIControl(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}

void UIControl::OnFocusGained() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusGained", GetName());
    m_keyboardFocused = true;
    MarkDirty();
}

void UIControl::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    m_keyboardFocused = false;
    m_activationKey = 0;
    MarkDirty();
}

void UIControl::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!m_keyboardFocused || !CanFocus() || (scanCode != ScanCode::Enter && scanCode != ScanCode::Space)) return;
    if (isDown) {
        if (!m_activationKey) {
            m_activationKey = scanCode;
            MarkDirty();
        }
    } else if (m_activationKey == scanCode) {
        m_activationKey = 0;
        MarkDirty();
        FireEvent(UIEventData{UIEventType::OnClick});
    }
}

void UIControl::OnEvent(const UIEventData& data) {
    const ControlState previous = m_state;

    switch (data.type) {
        case UIEventType::OnMouseEnter: m_state = ControlState::Hovered; break;
        case UIEventType::OnMouseLeave: m_state = ControlState::Normal; break;
        case UIEventType::OnMouseDown:
            m_state = ControlState::Pressed;
            data.handled = true;
            break;
        case UIEventType::OnMouseUp:
            m_state = (previous == ControlState::Pressed) ? ControlState::Hovered : previous;
            data.handled = true;
            break;
        default: break;
    }

    LAMBUI_LOGT(TAG, "'{}' OnEvent({}, handled={})", GetName(), ToString(data.type), data.handled);
    if (m_state != previous) {
        LAMBUI_LOGT(TAG, "'{}' state {} -> {}", GetName(), ToString(previous), ToString(m_state));
        MarkDirty();
        OnStateChanged(previous, m_state);
    }
}

} // namespace LambUI
