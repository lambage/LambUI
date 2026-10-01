#pragma once

#include "lambui_export.h"
#include "UIControl.h"
#include "UIInteractionInterfaces.h"
#include <string>

namespace LambUI {

// For player name fields, chat entry bars, or macro creation.
class LAMBUI_API UIInputBox : public UIControl, public IFocusable {
public:
    explicit UIInputBox(std::string name = {});

    bool IsFocused() const { return m_isFocused; }
    void AppendCharacter(char32_t codepoint);
    void Backspace();
    void SubmitEnter();

    const std::string& GetText() const { return m_text; }
    void SetText(const std::string& text);
    void SetFont(void* fontHandle);
    void* GetFont() const { return m_fontHandle; }

    // IFocusable
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnCharacter(char32_t codepoint) override { AppendCharacter(codepoint); }
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    std::string m_text;
    void* m_fontHandle = nullptr;
    bool m_isFocused = false;
};

} // namespace LambUI
