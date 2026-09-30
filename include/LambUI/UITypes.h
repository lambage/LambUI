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
    constexpr uint32_t Enter = 13;
    constexpr uint32_t Escape = 27;
} // namespace ScanCode

} // namespace LambUI
