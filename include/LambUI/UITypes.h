#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace LambUI {

enum class AnchorPoint {
    TopLeft, TopRight, BottomLeft, BottomRight,
    Center, Left, Right, Top, Bottom
};

enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2
};

inline const char* ToString(AnchorPoint point) {
    switch (point) {
        case AnchorPoint::TopLeft: return "TopLeft";
        case AnchorPoint::TopRight: return "TopRight";
        case AnchorPoint::BottomLeft: return "BottomLeft";
        case AnchorPoint::BottomRight: return "BottomRight";
        case AnchorPoint::Center: return "Center";
        case AnchorPoint::Left: return "Left";
        case AnchorPoint::Right: return "Right";
        case AnchorPoint::Top: return "Top";
        case AnchorPoint::Bottom: return "Bottom";
    }
    return "Unknown";
}

inline const char* ToString(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left";
        case MouseButton::Right: return "Right";
        case MouseButton::Middle: return "Middle";
    }
    return "Unknown";
}

enum class RenderCommandType {
    DrawQuad,
    DrawString,
    PushScissor,
    PopScissor,
    CustomCallback
};

// Absolute, resolved screen-space rectangle (top-left origin, pixels).
struct UIRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Payload executed for RenderCommandType::CustomCallback (e.g. an embedded 3D viewport).
struct UICustomRenderArgs {
    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    void* userData = nullptr;
};
using UICustomRenderCallback = std::function<void(const UICustomRenderArgs&)>;

// A single, flat draw instruction. Backends translate these into native draw
// calls; the library itself never issues a single graphics API call.
struct UIRenderCommand {
    RenderCommandType type = RenderCommandType::DrawQuad;

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;

    uint32_t color = 0xFFFFFFFFu; // Packed RGBA

    void* textureHandle = nullptr; // Opaque handle owned by the consuming renderer
    void* fontHandle = nullptr;    // Opaque handle owned by the consuming renderer
    std::string text;              // Valid only for DrawString commands

    UICustomRenderCallback customRenderFunc;
    void* customRenderUserData = nullptr;
};

// Minimal, engine-agnostic scan codes understood by focusable widgets
// (e.g. UIInputBox). Consumers translate their platform key codes to these
// before calling UIManager::InjectKeyEvent.
namespace ScanCode {
    constexpr uint32_t Backspace = 8;
    constexpr uint32_t Tab = 9;
    constexpr uint32_t Enter = 13;
    constexpr uint32_t Escape = 27;
    constexpr uint32_t Space = 32;
    constexpr uint32_t LeftShift = 0x100;
    constexpr uint32_t RightShift = 0x101;
    constexpr uint32_t Left = 0x102;
    constexpr uint32_t Right = 0x103;
    constexpr uint32_t Up = 0x104;
    constexpr uint32_t Down = 0x105;
    constexpr uint32_t Home = 0x106;
    constexpr uint32_t End = 0x107;
    constexpr uint32_t Delete = 0x108;
} // namespace ScanCode

} // namespace LambUI
