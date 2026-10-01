#pragma once

#include "UIContextMenu.h"

namespace LambUI {

class LAMBUI_API UIMenuBar : public UIWidget {
public:
    explicit UIMenuBar(UIManager& manager, std::string name = {});
    UIContextMenu* AddMenu(std::string label, std::vector<UIMenuItem> items);
    size_t GetMenuCount() const { return m_entries.size(); }
    UIContextMenu* GetMenu(size_t index) const;
    bool ClipsChildren() const override { return true; }

protected:
    void OnLayoutChanged() override;
    void GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    struct Entry { UIButton* button; UIContextMenu* menu; };
    void OpenMenu(size_t index);
    UIManager& m_manager;
    std::vector<Entry> m_entries;
};

} // namespace LambUI