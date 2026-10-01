#pragma once

#include "lambui_export.h"
#include "UIWidget.h"
#include "UIInteractionInterfaces.h"

namespace LambUI {

enum class ControlState { Normal, Hovered, Pressed };

inline const char* ToString(ControlState state) {
    switch (state) {
        case ControlState::Normal: return "Normal";
        case ControlState::Hovered: return "Hovered";
        case ControlState::Pressed: return "Pressed";
    }
    return "Unknown";
}

// Common base for widgets that react visually to mouse hover/press transitions.
class LAMBUI_API UIControl : public UIWidget, public IFocusable {
public:
    explicit UIControl(std::string name = {});

    ControlState GetState() const { return m_state; }
    bool HasKeyboardFocus() const { return m_keyboardFocused; }
    bool CanFocus() const override { return IsMouseEnabled(); }
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnCharacter(char32_t) override {}
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

protected:
    void OnEvent(const UIEventData& data) override;
    virtual void OnStateChanged(ControlState previous, ControlState current) { (void)previous; (void)current; }

private:
    ControlState m_state = ControlState::Normal;
    bool m_keyboardFocused = false;
    uint32_t m_activationKey = 0;
};

} // namespace LambUI
