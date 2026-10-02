#pragma once

#include "lambui_export.h"
#include "IRenderer.h"
#include "UIEvent.h"
#include "UITypes.h"
#include "UIWidget.h"

#include <map>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace LambUI {

class UITooltip;
class UIButton;

// Root context: owns the widget tree, drives layout resolution, performs
// input hit-testing/bubbling, and flushes render commands to the backend.
class LAMBUI_API UIManager {
public:
    explicit UIManager(std::shared_ptr<IRenderer> renderer,
                        std::shared_ptr<ITextMeasurer> textMeasurer = nullptr);
    ~UIManager();

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    UIWidget& GetRoot() { return *m_root; }
    const UIWidget& GetRoot() const { return *m_root; }
    const ITextMeasurer* GetTextMeasurer() const { return m_textMeasurer.get(); }

    void SetDisplaySize(float width, float height);
    UIWidget& GetOverlayRoot() { return *m_overlayRoot; }
    void ShowPopup(UIWidget& popup, float x, float y, UIWidget* owner = nullptr, bool allowOwnerInput = false);
    void ClosePopup();
    UIWidget* GetActivePopup() const { return m_activePopup; }
    void SetTooltipDelay(float seconds);

    // --- Input injection pipeline (engine pushes events in; library never polls) ---
    void InjectMouseMove(float x, float y);
    void InjectMouseLeave();
    PointerShape GetPointerShape() const;
    void InjectMouseButton(MouseButton button, bool isDown);
    void InjectMouseWheel(float xOffset, float yOffset);
    void InjectKeyEvent(uint32_t scanCode, bool isDown);
    void InjectCharacter(char32_t codepoint);
    void SetClipboardCallbacks(std::function<bool(std::string&)> read,
                               std::function<bool(const std::string&)> write);
    bool InjectCopy();
    bool InjectCut();
    bool InjectPaste();
    UIWidget* GetFocusedWidget() const { return m_focusedWidget; }
    bool SetDefaultButton(UIWidget& dialog, UIButton* button);
    UIButton* GetDefaultButton(const UIWidget& dialog) const;

    // --- Core frame loop ---
    void Update(float deltaTime);
    void Render();

    // --- Gameplay event bus (Observer pattern, e.g. "UNIT_HEALTH") ---
    void SubscribeGameEvent(const std::string& eventName, UIWidget* listener, UIGameEventCallback callback);
    void UnsubscribeGameEvent(const std::string& eventName, UIWidget* listener);
    void FireGameEvent(const std::string& eventName, void* payload = nullptr);

private:
    friend class UIContextMenu;
    struct Submenu {
        UIWidget* widget;
        UIWidget* parent;
        UIWidget* owner;
        float width;
        float height;
    };
    void ShowSubmenu(UIWidget& popup, UIWidget& parent, UIWidget& owner, bool focus);
    void CloseSubmenus(UIWidget& parent, bool restoreFocus = true);
    void PlaceSubmenu(const Submenu& submenu);
    bool IsPopupOpen(const UIWidget* popup) const;
    UIWidget* PopupScope() const;
    UIWidget* HitTestPopups(float x, float y) const;
    void SetFocusedWidget(UIWidget* widget);
    UIButton* DialogDefaultTarget() const;
    void CancelDialogDefaultPress();
    void ValidateDialogDefaultPress();
    void MoveFocus(bool backwards);
    void CollectFocusTargets(UIWidget& widget, std::vector<UIWidget*>& targets) const;
    UIWidget* HitTestRecursive(UIWidget& widget, float x, float y) const;
    void UpdateHover(float x, float y);
    UIWidget* HitTestInput(float x, float y) const;
    void PlaceOverlay(UIWidget& widget, float x, float y, float requestedWidth, float requestedHeight);
    void ResetTooltip();
    void UpdateTooltip(float deltaTime);

    struct GameEventSubscription {
        UIWidget* listener;
        UIGameEventCallback callback;
    };

    std::shared_ptr<IRenderer> m_renderer;
    std::function<bool(std::string&)> m_readClipboard;
    std::function<bool(const std::string&)> m_writeClipboard;
    std::shared_ptr<ITextMeasurer> m_textMeasurer;
    std::unique_ptr<UIWidget> m_root;
    std::unique_ptr<UIWidget> m_overlayRoot;
    UITooltip* m_tooltip = nullptr;
    UIWidget* m_activePopup = nullptr;
    std::vector<Submenu> m_submenus;
    UIWidget* m_popupOwner = nullptr;
    UIWidget* m_tooltipTarget = nullptr;
    float m_tooltipDelay = 0.5f;
    float m_tooltipElapsed = 0.0f;
    float m_popupX = 0.0f;
    float m_popupY = 0.0f;
    float m_popupWidth = 0.0f;
    float m_popupHeight = 0.0f;
    bool m_dismissedPopupPress = false;
    bool m_popupAllowsOwnerInput = false;
    std::vector<UIRenderCommand> m_commandBucket;
    std::multimap<std::string, GameEventSubscription> m_gameEventListeners;

    float m_displayWidth = 1920.0f;
    float m_displayHeight = 1080.0f;
    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;
    bool m_mouseInside = true;

    UIWidget* m_hoveredWidget = nullptr;
    UIWidget* m_pressedWidget = nullptr; // captures mouse-move for dragging (e.g. UISlider)
    MouseButton m_pressedButton = MouseButton::Left;
    UIWidget* m_focusedWidget = nullptr; // receives keyboard input (e.g. UIInputBox)
    UIWidget* m_popupPreviousFocus = nullptr;
    std::map<const UIWidget*, UIButton*> m_defaultButtons;
    UIButton* m_defaultPressedButton = nullptr;
    bool m_defaultEnterDown = false;
    bool m_leftShift = false;
    bool m_rightShift = false;
    bool m_leftControl = false;
    bool m_rightControl = false;
};

} // namespace LambUI
