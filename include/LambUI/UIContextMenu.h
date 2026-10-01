#pragma once

#include "UIScrollContainer.h"
#include <functional>

namespace LambUI {

class UIManager;
class UIButton;
class UITextWidget;
class UITextureWidget;

struct UIMenuItem {
    std::string label;
    std::function<void()> action;
    bool enabled = true;
    bool separator = false;
};

class LAMBUI_API UIContextMenu : public UIScrollContainer {
public:
    explicit UIContextMenu(UIManager& manager, std::string name = {});
    void SetItems(std::vector<UIMenuItem> items);
    const std::vector<UIMenuItem>& GetItems() const { return m_items; }
    void Open(float x, float y, UIWidget* owner = nullptr, bool allowOwnerInput = false);
    void Close();
    bool IsOpen() const;

protected:
    void OnLayoutChanged() override;
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    struct Row { UIButton* button; UITextWidget* label; UITextureWidget* separator; };
    void Activate(size_t index);
    UIManager& m_manager;
    std::vector<UIMenuItem> m_items;
    std::vector<Row> m_rows;
    static constexpr float RowHeight = 24.0f;
};

} // namespace LambUI