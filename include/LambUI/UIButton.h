#pragma once

#include "lambui_export.h"
#include "UIControl.h"
#include <cstdint>

namespace LambUI {

// Simple tri-state (normal/hover/pressed) clickable widget.
class LAMBUI_API UIButton : public UIControl {
public:
    explicit UIButton(std::string name = {});

    void SetTexture(void* textureHandle) { m_textureHandle = textureHandle; MarkDirty(); }
    void SetNormalColor(uint32_t color) { m_normalColor = color; MarkDirty(); }
    void SetHoverColor(uint32_t color) { m_hoverColor = color; MarkDirty(); }
    void SetPressedColor(uint32_t color) { m_pressedColor = color; MarkDirty(); }
    void SetKeyboardFocusColor(uint32_t color);

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    void* m_textureHandle = nullptr;
    uint32_t m_normalColor = 0xFF808080u;
    uint32_t m_hoverColor = 0xFFA0A0A0u;
    uint32_t m_pressedColor = 0xFF606060u;
    uint32_t m_focusColor = 0;
    bool m_hasFocusColor = false;
};

} // namespace LambUI
