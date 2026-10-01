#pragma once

#include "UIContextMenu.h"
#include "UIControl.h"

namespace LambUI {

class LAMBUI_API UIMenuBar : public UIControl {
public:
    explicit UIMenuBar(UIManager& manager, std::string name = {});
    UIContextMenu* AddMenu(std::string label, std::vector<UIMenuItem> items);
    size_t GetMenuCount() const { return m_entries.size(); }
    UIContextMenu* GetMenu(size_t index) const;
    bool ClipsChildren() const override { return true; }
    bool CanFocus() const override { return !m_entries.empty() && UIControl::CanFocus(); }
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

protected:
    void OnLayoutChanged() override;
    void GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    struct Entry { UIButton* button; UIContextMenu* menu; };
    void OpenMenu(size_t index);
    void HighlightHeader(bool highlighted);
    UIManager& m_manager;
    std::vector<Entry> m_entries;
    size_t m_selectedIndex = 0;
};

} // namespace LambUI