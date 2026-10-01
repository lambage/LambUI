#pragma once

#include "UIWidget.h"
#include "UIInteractionInterfaces.h"
#include <array>

namespace LambUI {

class UIScrollContainer;
enum class WindowState { Normal, Minimized, Maximized };
enum class WindowButton { Minimize, Maximize, Close };
enum class WindowButtonMode { Enabled, Disabled, Hidden };

inline const char* ToString(WindowState state) {
    switch (state) {
        case WindowState::Normal: return "Normal";
        case WindowState::Minimized: return "Minimized";
        case WindowState::Maximized: return "Maximized";
    }
    return "Unknown";
}
inline const char* ToString(WindowButton button) {
    switch (button) {
        case WindowButton::Minimize: return "Minimize";
        case WindowButton::Maximize: return "Maximize";
        case WindowButton::Close: return "Close";
    }
    return "Unknown";
}
inline const char* ToString(WindowButtonMode mode) {
    switch (mode) {
        case WindowButtonMode::Enabled: return "Enabled";
        case WindowButtonMode::Disabled: return "Disabled";
        case WindowButtonMode::Hidden: return "Hidden";
    }
    return "Unknown";
}

class LAMBUI_API UIWindow : public UIWidget, public IDraggable {
public:
    explicit UIWindow(std::string name = {});
    ~UIWindow() override;
    UIWidget* GetContent() const;
    void SetTitle(std::string title);
    const std::string& GetTitle() const { return m_title; }
    void SetMovable(bool movable);
    bool IsMovable() const { return m_movable; }
    void SetResizable(bool resizable);
    bool IsResizable() const { return m_resizable; }
    void SetButtonMode(WindowButton button, WindowButtonMode mode);
    WindowButtonMode GetButtonMode(WindowButton button) const;
    UIRect GetButtonRect(WindowButton button) const;
    void SetSizeLimits(float minWidth, float minHeight, float maxWidth, float maxHeight);
    void SetBounds(float x, float y, float width, float height);
    void Minimize();
    void Maximize();
    void Restore();
    void Close();
    WindowState GetWindowState() const { return m_state; }
    bool ClipsChildren() const override { return true; }
    void OnDrag(float mouseX, float mouseY) override;

protected:
    void OnPointerActivated() override;
    void OnLayoutChanged() override;
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;
    void GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    void ApplyBounds(const UIRect& bounds);
    void SaveNormalBounds();
    void CancelDrag();
    int ButtonAt(float x, float y) const;
    UIScrollContainer* m_client = nullptr;
    std::string m_title;
    std::array<WindowButtonMode, 3> m_buttons{WindowButtonMode::Enabled, WindowButtonMode::Enabled, WindowButtonMode::Enabled};
    WindowState m_state = WindowState::Normal;
    bool m_restoreMaximized = false;
    bool m_boundsPending = false;
    bool m_movable = true;
    bool m_resizable = true;
    bool m_dragging = false;
    bool m_moved = false;
    unsigned m_resizeEdges = 0;
    int m_pressedButton = -1;
    float m_startX = 0.0f;
    float m_startY = 0.0f;
    float m_minWidth = 180.0f;
    float m_minHeight = 96.0f;
    float m_maxWidth = 10000.0f;
    float m_maxHeight = 10000.0f;
    UIRect m_dragBounds;
    UIRect m_normalBounds{0.0f, 0.0f, 320.0f, 240.0f};
    static constexpr float TitleHeight = 32.0f;
    static constexpr float Border = 6.0f;
};
} // namespace LambUI