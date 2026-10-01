#pragma once

#include "lambui_export.h"
#include "UIEvent.h"
#include "UILayoutSolver.h"
#include "UILog.h"
#include "UITypes.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace LambUI {

class UIManager;

// Base node of the logical/retained widget tree. Owns its children, its
// anchor bindings, and its own draw-state; knows nothing about any graphics API.
class LAMBUI_API UIWidget {
public:
    friend class UIManager;

    explicit UIWidget(std::string name = {});
    virtual ~UIWidget();

    UIWidget(const UIWidget&) = delete;
    UIWidget& operator=(const UIWidget&) = delete;

    template <typename TWidget, typename... TArgs>
    TWidget* CreateChild(TArgs&&... args) {
        static_assert(std::is_base_of_v<UIWidget, TWidget>, "TWidget must derive from UIWidget");
        auto child = std::make_unique<TWidget>(std::forward<TArgs>(args)...);
        TWidget* raw = child.get();
        child->m_parent = this;
        child->MarkDirty();
        LAMBUI_LOGT("UIWidget", "CreateChild: '{}' parented to '{}'", raw->GetName(), m_name);
        m_children.push_back(std::move(child));
        return raw;
    }

    // --- Layout (WoW-style point-and-anchor system) ---
    void SetPoint(AnchorPoint myPoint, UIWidget* relativeTo, AnchorPoint relativePoint,
                  float xOffset = 0.0f, float yOffset = 0.0f);
    void SetAllPoints(UIWidget* relativeTo);
    void ClearPoints();
    void BringToFront();
    void SetSize(float width, float height);
    const UIRect& GetComputedRect() const { return m_computedRect; }

    // --- Visibility / input state ---
    void SetVisible(bool visible);
    bool IsVisible() const { return m_isVisible; }
    void SetMouseEnabled(bool enabled) { m_isMouseEnabled = enabled; }
    bool IsMouseEnabled() const { return m_isMouseEnabled; }
    void SetKeyboardEnabled(bool enabled);
    bool IsKeyboardEnabled() const { return m_isKeyboardEnabled; }
    virtual bool ClipsChildren() const { return false; }
    void SetTooltip(std::string text);
    const std::string& GetTooltip() const { return m_tooltip; }

    const std::string& GetName() const { return m_name; }
    UIWidget* GetParent() const { return m_parent; }

    // --- Events ---
    void RegisterCallback(UIEventType type, UIEventCallback callback);
    void FireEvent(const UIEventData& data);
    bool HitTest(float x, float y) const;

    void MarkDirty();
    bool IsDirty() const { return m_isDirty; }

protected:
    virtual void OnPointerActivated() {}
    virtual void OnLayoutChanged() {}

    // Overridden by concrete widgets to react to state transitions (hover/press/etc).
    virtual void OnEvent(const UIEventData& /*data*/) {}

    // Overridden by concrete widgets to translate their own state into draw
    // commands; children are still walked automatically by the caller.
    virtual void OnGenerateRenderCommands(std::vector<UIRenderCommand>& /*bucket*/) {}

    // Walks self (via OnGenerateRenderCommands) then children; overridden by
    // container widgets that need to wrap their subtree (e.g.
    // UIScrollContainer emitting PushScissor/PopScissor around its content).
    virtual void GenerateRenderCommands(std::vector<UIRenderCommand>& bucket);
    void AppendChildRenderCommands(UIWidget& child, std::vector<UIRenderCommand>& bucket);

    const std::vector<std::unique_ptr<UIWidget>>& GetChildren() const { return m_children; }
    const std::vector<std::unique_ptr<UIWidget>>& GetSiblings() const {
        static const std::vector<std::unique_ptr<UIWidget>> empty;
        return m_parent ? m_parent->m_children : empty;
    }

private:
    struct AnchorBinding {
        AnchorPoint myPoint;
        UIWidget* relativeTo;
        AnchorPoint relativePoint;
        float xOffset;
        float yOffset;
    };

    // Only UIManager drives these; they require top-down tree traversal.
    void ResolveLayout();
    void SetComputedRectDirect(const UIRect& rect);

    std::string m_name;
    std::string m_tooltip;
    UIWidget* m_parent = nullptr;
    std::vector<std::unique_ptr<UIWidget>> m_children;
    std::vector<AnchorBinding> m_anchors;
    std::map<UIEventType, UIEventCallback> m_callbacks;

    UIRect m_computedRect;
    float m_width = 0.0f;
    float m_height = 0.0f;
    bool m_isVisible = true;
    bool m_isMouseEnabled = true;
    bool m_isKeyboardEnabled = true;
    bool m_isDirty = true;
};

} // namespace LambUI
