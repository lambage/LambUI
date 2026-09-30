#include "LambUI/UIManager.h"
#include "LambUI/UIInteractionInterfaces.h"

namespace LambUI {

UIManager::UIManager(std::shared_ptr<IRenderer> renderer, std::shared_ptr<ITextMeasurer> textMeasurer)
    : m_renderer(std::move(renderer)),
      m_textMeasurer(std::move(textMeasurer)),
      m_root(std::make_unique<UIWidget>("Root")) {
    m_root->SetComputedRectDirect(UIRect{0.0f, 0.0f, m_displayWidth, m_displayHeight});
}

UIManager::~UIManager() = default;

void UIManager::SetDisplaySize(float width, float height) {
    m_displayWidth = width;
    m_displayHeight = height;
    m_root->SetComputedRectDirect(UIRect{0.0f, 0.0f, width, height});
}

void UIManager::InjectMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;

    if (m_pressedWidget) {
        // Widget that captured the press keeps movement; no hover churn underneath.
        if (auto* draggable = dynamic_cast<IDraggable*>(m_pressedWidget)) {
            draggable->OnDrag(x, y);
        }
        return;
    }

    UpdateHover(x, y);
}

void UIManager::UpdateHover(float x, float y) {
    UIWidget* target = HitTestRecursive(*m_root, x, y);

    if (target != m_hoveredWidget) {
        if (m_hoveredWidget) {
            m_hoveredWidget->FireEvent(UIEventData{UIEventType::OnMouseLeave, x, y});
        }
        if (target) {
            target->FireEvent(UIEventData{UIEventType::OnMouseEnter, x, y});
        }
        m_hoveredWidget = target;
    }
}

UIWidget* UIManager::HitTestRecursive(UIWidget& widget, float x, float y) const {
    // Reverse child order == front-to-back in render order; test top-most first.
    const auto& children = widget.GetChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (UIWidget* hit = HitTestRecursive(**it, x, y)) {
            return hit;
        }
    }
    return widget.HitTest(x, y) ? &widget : nullptr;
}

void UIManager::InjectMouseButton(MouseButton button, bool isDown) {
    UIWidget* target = HitTestRecursive(*m_root, m_mouseX, m_mouseY);

    if (isDown) {
        m_pressedWidget = target;

        if (target != m_focusedWidget) {
            if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
                focusable->OnFocusLost();
            }
            m_focusedWidget = target;
            if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
                focusable->OnFocusGained();
            }
        }

        if (target) {
            target->FireEvent(UIEventData{UIEventType::OnMouseDown, m_mouseX, m_mouseY, button, true});
        }
    } else {
        if (m_pressedWidget) {
            m_pressedWidget->FireEvent(UIEventData{UIEventType::OnMouseUp, m_mouseX, m_mouseY, button, false});
            if (m_pressedWidget == target) {
                m_pressedWidget->FireEvent(UIEventData{UIEventType::OnClick, m_mouseX, m_mouseY, button, false});
            }
        }
        m_pressedWidget = nullptr;
        UpdateHover(m_mouseX, m_mouseY);
    }
}

void UIManager::InjectMouseWheel(float /*xOffset*/, float /*yOffset*/) {
    // Reserved for scrollable containers (not yet implemented).
}

void UIManager::InjectKeyEvent(uint32_t scanCode, bool isDown) {
    if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
        focusable->OnKeyEvent(scanCode, isDown);
    }
}

void UIManager::InjectCharacter(char32_t codepoint) {
    if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
        focusable->OnCharacter(codepoint);
    }
}

void UIManager::Update(float /*deltaTime*/) {
    m_root->ResolveLayout();
}

void UIManager::Render() {
    m_commandBucket.clear();
    m_root->GenerateRenderCommands(m_commandBucket);
    if (m_renderer) {
        m_renderer->SubmitRenderCommands(m_commandBucket);
    }
}

void UIManager::SubscribeGameEvent(const std::string& eventName, UIWidget* listener, UIGameEventCallback callback) {
    m_gameEventListeners.emplace(eventName, GameEventSubscription{listener, std::move(callback)});
}

void UIManager::UnsubscribeGameEvent(const std::string& eventName, UIWidget* listener) {
    auto range = m_gameEventListeners.equal_range(eventName);
    for (auto it = range.first; it != range.second;) {
        if (it->second.listener == listener) {
            it = m_gameEventListeners.erase(it);
        } else {
            ++it;
        }
    }
}

void UIManager::FireGameEvent(const std::string& eventName, void* payload) {
    auto range = m_gameEventListeners.equal_range(eventName);
    for (auto it = range.first; it != range.second; ++it) {
        it->second.callback(payload);
    }
}

} // namespace LambUI
