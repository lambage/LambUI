#pragma once

#include "UIWidget.h"

namespace LambUI {

enum class ProgressOrientation { Horizontal, Vertical };

inline const char* ToString(ProgressOrientation orientation) {
    switch (orientation) {
        case ProgressOrientation::Horizontal: return "Horizontal";
        case ProgressOrientation::Vertical: return "Vertical";
    }
    return "Unknown";
}

class LAMBUI_API UIProgressBar : public UIWidget {
public:
    explicit UIProgressBar(std::string name = {});
    void SetMinMaxValues(float minValue, float maxValue);
    void SetValue(float value);
    float GetValue() const { return m_value; }
    float GetMinValue() const { return m_minValue; }
    float GetMaxValue() const { return m_maxValue; }
    void SetOrientation(ProgressOrientation orientation);
    ProgressOrientation GetOrientation() const { return m_orientation; }
    void SetColors(uint32_t background, uint32_t fill);

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    float m_minValue = 0.0f;
    float m_maxValue = 1.0f;
    float m_value = 0.0f;
    ProgressOrientation m_orientation = ProgressOrientation::Horizontal;
    uint32_t m_background = 0x404040FFu;
    uint32_t m_fill = 0x60B060FFu;
};

} // namespace LambUI