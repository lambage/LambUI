#pragma once

#include "lambui_export.h"
#include "UIInteractionInterfaces.h"
#include "UIWidget.h"

namespace LambUI {

// A fixed-size clipped viewport onto an internal "content" widget that can be
// larger than the viewport; mouse-wheel input pans the content, and
// PushScissor/PopScissor clip anything outside the viewport rect. Widgets
// meant to scroll must be created as children of GetContent(), not of the
// container itself.
class LAMBUI_API UIScrollContainer : public UIWidget, public IScrollable {
public:
    explicit UIScrollContainer(std::string name = {});

    UIWidget* GetContent() { return m_content; }

    // Explicit size of the pannable content area (no auto-measurement from
    // children bounds - matches the rest of the library's explicit-size model).
    void SetContentSize(float width, float height);

    void SetScrollOffset(float x, float y);
    void EnsureVisible(const UIRect& rect);
    float GetScrollX() const { return m_scrollX; }
    float GetScrollY() const { return m_scrollY; }
    bool ClipsChildren() const override { return true; }

    void SetScrollbarsEnabled(bool enabled);
    bool AreScrollbarsEnabled() const { return m_scrollbarsEnabled; }

    // IScrollable: pixel deltas; positive values move toward the left/top.
    // Hosts convert wheel steps to pixels before UIManager::InjectMouseWheel.
    void OnScroll(float xOffset, float yOffset) override;

protected:
    void OnLayoutChanged() override;
    void GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    class ScrollBar;
    void ApplyScrollOffset();
    void UpdateScrollbars();

    UIWidget* m_content = nullptr;
    ScrollBar* m_horizontalBar = nullptr;
    ScrollBar* m_verticalBar = nullptr;
    bool m_scrollbarsEnabled = true;
    float m_contentWidth = 0.0f;
    float m_contentHeight = 0.0f;
    float m_scrollX = 0.0f;
    float m_scrollY = 0.0f;
};

} // namespace LambUI
