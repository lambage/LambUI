#pragma once

#include "lambui_export.h"
#include "IRenderer.h"
#include "UIEvent.h"
#include "UITypes.h"
#include "UIWidget.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace LambUI {

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

    // --- Input injection pipeline (engine pushes events in; library never polls) ---
    void InjectMouseMove(float x, float y);
    void InjectMouseButton(MouseButton button, bool isDown);
    void InjectMouseWheel(float xOffset, float yOffset);
    void InjectKeyEvent(uint32_t scanCode, bool isDown);
    void InjectCharacter(char32_t codepoint);

    // --- Core frame loop ---
    void Update(float deltaTime);
    void Render();

    // --- Gameplay event bus (Observer pattern, e.g. "UNIT_HEALTH") ---
    void SubscribeGameEvent(const std::string& eventName, UIWidget* listener, UIGameEventCallback callback);
    void UnsubscribeGameEvent(const std::string& eventName, UIWidget* listener);
    void FireGameEvent(const std::string& eventName, void* payload = nullptr);

private:
    UIWidget* HitTestRecursive(UIWidget& widget, float x, float y) const;
    void UpdateHover(float x, float y);

    struct GameEventSubscription {
        UIWidget* listener;
        UIGameEventCallback callback;
    };

    std::shared_ptr<IRenderer> m_renderer;
    std::shared_ptr<ITextMeasurer> m_textMeasurer;
    std::unique_ptr<UIWidget> m_root;
    std::vector<UIRenderCommand> m_commandBucket;
    std::multimap<std::string, GameEventSubscription> m_gameEventListeners;

    float m_displayWidth = 1920.0f;
    float m_displayHeight = 1080.0f;
    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;

    UIWidget* m_hoveredWidget = nullptr;
    UIWidget* m_pressedWidget = nullptr; // captures mouse-move for dragging (e.g. UISlider)
    UIWidget* m_focusedWidget = nullptr; // receives keyboard input (e.g. UIInputBox)
};

} // namespace LambUI
