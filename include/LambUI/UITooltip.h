#pragma once

#include "UIWidget.h"

namespace LambUI {

class ITextMeasurer;

class LAMBUI_API UITooltip : public UIWidget {
public:
    explicit UITooltip(std::string name = {});
    void SetText(std::string text, const ITextMeasurer* measurer = nullptr, float maximumWidth = 320.0f);
    const std::string& GetText() const { return m_text; }

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    std::string m_text;
    std::vector<std::string> m_lines;
    float m_lineHeight = 16.0f;
};

} // namespace LambUI