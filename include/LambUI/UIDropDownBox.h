#pragma once

#include "lambui_export.h"
#include "UIButton.h"
#include "UIControl.h"
#include <string>
#include <vector>

namespace LambUI {

class UITextWidget;

// A combination widget: a button showing the current selection, which
// expands a vertical stack of option buttons (its own children) when clicked.
class LAMBUI_API UIDropDownBox : public UIControl {
public:
    explicit UIDropDownBox(std::string name = {});

    void SetOptions(std::vector<std::string> options);
    void SetSelectedIndex(int index);
    int GetSelectedIndex() const { return m_selectedIndex; }
    const std::string& GetSelectedOption() const;

    void Toggle();
    bool IsExpanded() const { return m_isExpanded; }

protected:
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    void RebuildOptionButtons();

    std::vector<std::string> m_options;
    int m_selectedIndex = -1;
    bool m_isExpanded = false;
    std::vector<UIButton*> m_optionButtons; // owned by the widget tree (children)
    std::vector<UITextWidget*> m_optionLabels;
};

} // namespace LambUI
