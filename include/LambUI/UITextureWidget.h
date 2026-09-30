#pragma once

#include "lambui_export.h"
#include "UIWidget.h"
#include <cstdint>

namespace LambUI {

// Displays a single opaque texture handle as a quad; the handle's meaning
// (GL id, VkImageView, D3D12 descriptor, etc.) is owned entirely by the renderer.
class LAMBUI_API UITextureWidget : public UIWidget {
public:
    explicit UITextureWidget(std::string name = {});

    void SetTexture(void* textureHandle) { m_textureHandle = textureHandle; MarkDirty(); }
    void SetUVRect(float u0, float v0, float u1, float v1);
    void SetTint(uint32_t color) { m_tint = color; MarkDirty(); }

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    void* m_textureHandle = nullptr;
    float m_u0 = 0.0f, m_v0 = 0.0f, m_u1 = 1.0f, m_v1 = 1.0f;
    uint32_t m_tint = 0xFFFFFFFFu;
};

} // namespace LambUI
