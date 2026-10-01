#pragma once

#include "UIControl.h"

namespace LambUI {

class LAMBUI_API UITabControl : public UIControl {
public:
    explicit UITabControl(std::string name = {});
    UIWidget* AddTab(std::string label);
    void SetTabText(int index, std::string label);
    void SetSelectedIndex(int index);
    int GetSelectedIndex() const { return m_selectedIndex; }
    size_t GetTabCount() const { return m_tabs.size(); }
    UIWidget* GetPage(int index) const;
    bool ClipsChildren() const override { return true; }

protected:
    void OnLayoutChanged() override;
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;
    void GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    struct Tab { std::string label; UIWidget* page; };
    std::vector<Tab> m_tabs;
    int m_selectedIndex = -1;
    static constexpr float HeaderHeight = 28.0f;
};

} // namespace LambUI