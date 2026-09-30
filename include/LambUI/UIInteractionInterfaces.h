#pragma once

#include "lambui_export.h"
#include <cstdint>

namespace LambUI {

// Implemented by widgets that want continuous mouse-move updates while the
// mouse button remains held after a press began on them (e.g. UISlider).
class LAMBUI_API IDraggable {
public:
    virtual ~IDraggable() = default;
    virtual void OnDrag(float mouseX, float mouseY) = 0;
};

// Implemented by widgets that want raw keyboard/text input while focused
// (e.g. UIInputBox).
class LAMBUI_API IFocusable {
public:
    virtual ~IFocusable() = default;
    virtual void OnFocusGained() {}
    virtual void OnFocusLost() {}
    virtual void OnCharacter(char32_t codepoint) = 0;
    virtual void OnKeyEvent(uint32_t scanCode, bool isDown) = 0;
};

} // namespace LambUI
