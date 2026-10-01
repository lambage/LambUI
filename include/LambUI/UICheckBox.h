#pragma once

#include "UIControl.h"

namespace LambUI {

class LAMBUI_API UICheckBox : public UIControl {
public:
    explicit UICheckBox(std::string name = {});
    ~UICheckBox() override;
    void SetText(std::string text);
    const std::string& GetText() const { return m_text; }
    virtual void SetChecked(bool checked);
    bool IsChecked() const { return m_checked; }
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }
    bool CanFocus() const override { return m_enabled && UIControl::CanFocus(); }
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnCharacter(char32_t) override {}
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

protected:
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;
    virtual void GenerateIndicator(std::vector<UIRenderCommand>& bucket, float x, float y, uint32_t color);
    virtual void Activate();
    bool AssignChecked(bool checked);

private:
    std::string m_text;
    bool m_checked = false;
    bool m_enabled = true;
    bool m_focused = false;
    uint32_t m_pressedKey = 0;
};

} // namespace LambUI