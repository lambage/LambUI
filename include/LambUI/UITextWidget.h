#pragma once

#include "lambui_export.h"
#include "UIWidget.h"
#include <cstdint>
#include <string>

namespace LambUI {

class ITextMeasurer;

// Renders a string via the backend's font/SDF pipeline; the library only
// stores the string and (optionally) requests metrics through ITextMeasurer.
class LAMBUI_API UITextWidget : public UIWidget {
public:
    explicit UITextWidget(std::string name = {});

    void SetText(const std::string& text);
    const std::string& GetText() const { return m_text; }

    void SetFont(void* fontHandle);
    void* GetFont() const { return m_fontHandle; }
    void SetColor(uint32_t color) { m_color = color; MarkDirty(); }

    // Optional: when set, the widget resizes itself to fit the text on change.
    void SetTextMeasurer(const ITextMeasurer* measurer);

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    void UpdateTextSize();

    std::string m_text;
    void* m_fontHandle = nullptr;
    uint32_t m_color = 0xFFFFFFFFu;
    const ITextMeasurer* m_textMeasurer = nullptr;
};

} // namespace LambUI
