#pragma once

#include "lambui_export.h"
#include "UIWidget.h"

namespace LambUI {

enum class ControlState { Normal, Hovered, Pressed };

// Common base for widgets that react visually to mouse hover/press transitions.
class LAMBUI_API UIControl : public UIWidget {
public:
    explicit UIControl(std::string name = {});

    ControlState GetState() const { return m_state; }

protected:
    void OnEvent(const UIEventData& data) override;
    virtual void OnStateChanged(ControlState previous, ControlState current) { (void)previous; (void)current; }

private:
    ControlState m_state = ControlState::Normal;
};

} // namespace LambUI
