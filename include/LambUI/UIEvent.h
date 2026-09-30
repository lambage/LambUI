#pragma once

#include "UITypes.h"
#include <functional>

namespace LambUI {

enum class UIEventType {
    OnMouseEnter,
    OnMouseLeave,
    OnMouseDown,
    OnMouseUp,
    OnClick,
    OnValueChanged,
    OnTextChanged,
    OnEnterPressed
};

struct UIEventData {
    UIEventType type = UIEventType::OnClick;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    MouseButton button = MouseButton::Left;
    bool isDown = false;
};

using UIEventCallback = std::function<void(const UIEventData&)>;
using UIGameEventCallback = std::function<void(void* payload)>;

} // namespace LambUI
