#pragma once

#include "lambui_export.h"
#include "UIControl.h"
#include "UIInteractionInterfaces.h"

namespace LambUI {

enum class SliderOrientation { Horizontal, Vertical };

// For options menus (volume, brightness, etc.) or gameplay resource bars.
class LAMBUI_API UISlider : public UIControl, public IDraggable {
public:
    explicit UISlider(std::string name = {});

    void SetOrientation(SliderOrientation orientation) { m_orientation = orientation; MarkDirty(); }
    void SetMinMaxValues(float minValue, float maxValue);
    void SetValue(float value);
    float GetValue() const { return m_value; }
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

    // IDraggable: called by UIManager while this widget has captured the mouse.
    void OnDrag(float mouseX, float mouseY) override;

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    SliderOrientation m_orientation = SliderOrientation::Horizontal;
    float m_minValue = 0.0f;
    float m_maxValue = 1.0f;
    float m_value = 0.0f;
};

} // namespace LambUI
