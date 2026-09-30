#pragma once

#include "lambui_export.h"
#include "UITypes.h"
#include <string>
#include <vector>

namespace LambUI {

// Hardware Abstraction Layer boundary. The library never includes a graphics
// API header; consumers implement this against Vulkan/D3D12/OpenGL/etc.
class LAMBUI_API IRenderer {
public:
    virtual ~IRenderer() = default;

    // Called once per frame with the fully resolved, flat command bucket.
    virtual void SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) = 0;
};

// Kept separate from IRenderer (Interface Segregation): text layout needs
// glyph metrics during Update(), before any rendering has happened.
class LAMBUI_API ITextMeasurer {
public:
    virtual ~ITextMeasurer() = default;

    virtual void MeasureText(const std::string& text, void* fontHandle,
                              float& outWidth, float& outHeight) const = 0;
};

} // namespace LambUI
