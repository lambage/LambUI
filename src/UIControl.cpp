#include "LambUI/UIControl.h"

namespace LambUI {

UIControl::UIControl(std::string name) : UIWidget(std::move(name)) {}

void UIControl::OnEvent(const UIEventData& data) {
    const ControlState previous = m_state;

    switch (data.type) {
        case UIEventType::OnMouseEnter: m_state = ControlState::Hovered; break;
        case UIEventType::OnMouseLeave: m_state = ControlState::Normal; break;
        case UIEventType::OnMouseDown: m_state = ControlState::Pressed; break;
        case UIEventType::OnMouseUp:
            m_state = (previous == ControlState::Pressed) ? ControlState::Hovered : previous;
            break;
        default: break;
    }

    if (m_state != previous) {
        MarkDirty();
        OnStateChanged(previous, m_state);
    }
}

} // namespace LambUI
