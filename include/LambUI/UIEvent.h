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

inline const char* ToString(UIEventType type) {
    switch (type) {
        case UIEventType::OnMouseEnter: return "OnMouseEnter";
        case UIEventType::OnMouseLeave: return "OnMouseLeave";
        case UIEventType::OnMouseDown: return "OnMouseDown";
        case UIEventType::OnMouseUp: return "OnMouseUp";
        case UIEventType::OnClick: return "OnClick";
        case UIEventType::OnValueChanged: return "OnValueChanged";
        case UIEventType::OnTextChanged: return "OnTextChanged";
        case UIEventType::OnEnterPressed: return "OnEnterPressed";
    }
    return "Unknown";
}

struct UIEventData {
    UIEventType type = UIEventType::OnClick;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    MouseButton button = MouseButton::Left;
    bool isDown = false;
    mutable bool handled = false;
};

using UIEventCallback = std::function<void(const UIEventData&)>;
using UIGameEventCallback = std::function<void(void* payload)>;

} // namespace LambUI
