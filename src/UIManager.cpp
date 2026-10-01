#include "LambUI/UIManager.h"
#include "LambUI/UIInteractionInterfaces.h"
#include "LambUI/UITooltip.h"
#include "LambUI/UIScrollContainer.h"
#include "LambUI/UIContextMenu.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace {
constexpr const char* TAG = "UIManager";

bool IsEffectivelyVisible(const UIWidget* widget) {
    for (auto* ancestor = widget; ancestor; ancestor = ancestor->GetParent()) {
        if (!ancestor->IsVisible()) return false;
    }
    return widget != nullptr;
}

bool IsWithin(const UIWidget* widget, const UIWidget* ancestor) {
    if (!ancestor) return false;
    for (auto* current = widget; current; current = current->GetParent()) {
        if (current == ancestor) return true;
    }
    return false;
}

bool CanFocusWidget(UIWidget* widget) {
    auto* focusable = dynamic_cast<IFocusable*>(widget);
    return IsEffectivelyVisible(widget) && widget->IsKeyboardEnabled() && focusable && focusable->CanFocus();
}

} // namespace

UIManager::UIManager(std::shared_ptr<IRenderer> renderer, std::shared_ptr<ITextMeasurer> textMeasurer)
    : m_renderer(std::move(renderer)),
      m_textMeasurer(std::move(textMeasurer)),
    m_root(std::make_unique<UIWidget>("Root")),
    m_overlayRoot(std::make_unique<UIWidget>("Overlays")) {
    LAMBUI_LOGD(TAG, "constructed (renderer={}, textMeasurer={})",
                fmt::ptr(m_renderer.get()), fmt::ptr(m_textMeasurer.get()));
    m_root->SetComputedRectDirect(UIRect{0.0f, 0.0f, m_displayWidth, m_displayHeight});
    m_overlayRoot->m_parent = m_root.get();
    m_overlayRoot->SetMouseEnabled(false);
    m_overlayRoot->SetComputedRectDirect(m_root->GetComputedRect());
    m_tooltip = m_overlayRoot->CreateChild<UITooltip>("Tooltip");
}

UIManager::~UIManager() {
    LAMBUI_LOGD(TAG, "destroyed");
}

void UIManager::SetDisplaySize(float width, float height) {
    LAMBUI_LOGD(TAG, "SetDisplaySize({}, {})", width, height);
    m_displayWidth = width;
    m_displayHeight = height;
    m_root->SetComputedRectDirect(UIRect{0.0f, 0.0f, width, height});
    m_overlayRoot->SetComputedRectDirect(m_root->GetComputedRect());
    ResetTooltip();
}

void UIManager::PlaceOverlay(UIWidget& widget, float x, float y, float requestedWidth, float requestedHeight) {
    LAMBUI_LOGT(TAG, "PlaceOverlay('{}', {}, {})", widget.GetName(), x, y);
    const float width = std::clamp(requestedWidth, 0.0f, std::max(0.0f, m_displayWidth));
    const float height = std::clamp(requestedHeight, 0.0f, std::max(0.0f, m_displayHeight));
    widget.SetSize(width, height);
    widget.ClearPoints();
    widget.SetPoint(AnchorPoint::TopLeft, m_overlayRoot.get(), AnchorPoint::TopLeft,
                   std::clamp(x, 0.0f, std::max(0.0f, m_displayWidth - width)),
                   std::clamp(y, 0.0f, std::max(0.0f, m_displayHeight - height)));
    widget.ResolveLayout();
}

void UIManager::ShowPopup(UIWidget& popup, float x, float y, UIWidget* owner, bool allowOwnerInput) {
    LAMBUI_LOGT(TAG, "ShowPopup('{}', {}, {})", popup.GetName(), x, y);
    if (popup.GetParent() != m_overlayRoot.get() || !std::isfinite(x) || !std::isfinite(y)) return;
    ClosePopup();
    ResetTooltip();
    m_activePopup = &popup;
    m_popupPreviousFocus = m_focusedWidget;
    m_popupOwner = owner;
    m_popupAllowsOwnerInput = allowOwnerInput;
    m_popupX = x;
    m_popupY = y;
    popup.SetVisible(true);
    m_popupWidth = popup.m_width;
    m_popupHeight = popup.m_height;
    PlaceOverlay(popup, x, y, m_popupWidth, m_popupHeight);
    SetFocusedWidget(nullptr);
    MoveFocus(false);
}

void UIManager::ClosePopup() {
    LAMBUI_LOGT(TAG, "ClosePopup");
    if (!m_activePopup) return;
    m_activePopup->SetVisible(false);
    m_activePopup->SetSize(m_popupWidth, m_popupHeight);
    m_activePopup = nullptr;
    m_popupOwner = nullptr;
    SetFocusedWidget(CanFocusWidget(m_popupPreviousFocus) ? m_popupPreviousFocus : nullptr);
    m_popupPreviousFocus = nullptr;
}

void UIManager::SetTooltipDelay(float seconds) {
    LAMBUI_LOGT(TAG, "SetTooltipDelay({})", seconds);
    if (!std::isfinite(seconds)) return;
    m_tooltipDelay = std::max(0.0f, seconds);
    ResetTooltip();
}

void UIManager::ResetTooltip() {
    LAMBUI_LOGT(TAG, "ResetTooltip");
    m_tooltip->SetVisible(false);
    m_tooltipTarget = nullptr;
    m_tooltipElapsed = 0.0f;
}

void UIManager::UpdateTooltip(float deltaTime) {
    if (m_activePopup || m_pressedWidget || m_dismissedPopupPress) {
        ResetTooltip();
        return;
    }
    UIWidget* target = m_hoveredWidget;
    while (target && target->GetTooltip().empty()) target = target->GetParent();
    if (target != m_tooltipTarget || (target && target->GetTooltip() != m_tooltip->GetText())) {
        ResetTooltip();
        m_tooltipTarget = target;
        if (target) m_tooltip->SetText(target->GetTooltip(), m_textMeasurer.get());
    }
    if (!target) return;
    if (std::isfinite(deltaTime) && deltaTime > 0.0f) m_tooltipElapsed += deltaTime;
    if (m_tooltipElapsed >= m_tooltipDelay && !m_tooltip->IsVisible()) {
        LAMBUI_LOGT(TAG, "ShowTooltip('{}')", target->GetName());
        PlaceOverlay(*m_tooltip, m_mouseX + 12.0f, m_mouseY + 18.0f, m_tooltip->m_width, m_tooltip->m_height);
        m_tooltip->SetVisible(true);
    }
}

UIWidget* UIManager::HitTestInput(float x, float y) const {
    if (m_activePopup) {
        if (auto* hit = HitTestRecursive(*m_activePopup, x, y)) return hit;
    }
    return HitTestRecursive(*m_root, x, y);
}

void UIManager::InjectMouseMove(float x, float y) {
    LAMBUI_LOGT(TAG, "InjectMouseMove({}, {})", x, y);
    m_mouseX = x;
    m_mouseY = y;

    if (IsEffectivelyVisible(m_pressedWidget)) {
        // Widget that captured the press keeps movement; no hover churn underneath.
        if (auto* draggable = dynamic_cast<IDraggable*>(m_pressedWidget)) {
            draggable->OnDrag(x, y);
        }
        return;
    }

    UpdateHover(x, y);
}

void UIManager::UpdateHover(float x, float y) {
    UIWidget* target = HitTestInput(x, y);

    if (target != m_hoveredWidget) {
        ResetTooltip();
        LAMBUI_LOGT(TAG, "hover '{}' -> '{}'", m_hoveredWidget ? m_hoveredWidget->GetName() : "<none>",
                    target ? target->GetName() : "<none>");
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
    if (!widget.IsVisible()) return nullptr;
    if (widget.ClipsChildren()) {
        const UIRect& rect = widget.GetComputedRect();
        if (rect.width <= 0.0f || rect.height <= 0.0f ||
            x < rect.x || x >= rect.x + rect.width ||
            y < rect.y || y >= rect.y + rect.height) {
            return nullptr;
        }
    }
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
    LAMBUI_LOGT(TAG, "InjectMouseButton({}, isDown={})", ToString(button), isDown);
    if (m_pressedWidget && button != m_pressedButton) return;
    ResetTooltip();
    if (isDown && m_activePopup && !HitTestRecursive(*m_activePopup, m_mouseX, m_mouseY)) {
        bool ownerHit = false;
        for (auto* hit = HitTestRecursive(*m_root, m_mouseX, m_mouseY); hit; hit = hit->GetParent()) {
            if (hit == m_popupOwner && m_popupAllowsOwnerInput) ownerHit = true;
        }
        if (!ownerHit) {
            ClosePopup();
            m_pressedWidget = nullptr;
            m_dismissedPopupPress = true;
            UpdateHover(m_mouseX, m_mouseY);
            return;
        }
    }
    if (m_dismissedPopupPress) {
        if (!isDown) m_dismissedPopupPress = false;
        return;
    }
    UIWidget* target = HitTestInput(m_mouseX, m_mouseY);

    if (isDown) {
        m_pressedWidget = target;
        m_pressedButton = button;
        for (auto* widget = target; widget; widget = widget->GetParent()) widget->OnPointerActivated();

        auto* focusTarget = target;
        while (focusTarget && !CanFocusWidget(focusTarget)) focusTarget = focusTarget->GetParent();
        SetFocusedWidget(focusTarget);

        if (target) {
            target->FireEvent(UIEventData{UIEventType::OnMouseDown, m_mouseX, m_mouseY, button, true});
        }
    } else {
        if (IsEffectivelyVisible(m_pressedWidget)) {
            m_pressedWidget->FireEvent(UIEventData{UIEventType::OnMouseUp, m_mouseX, m_mouseY, button, false});
            if (m_pressedWidget == target) {
                m_pressedWidget->FireEvent(UIEventData{UIEventType::OnClick, m_mouseX, m_mouseY, button, false});
            }
        }
        m_pressedWidget = nullptr;
        UpdateHover(m_mouseX, m_mouseY);
    }
}

void UIManager::InjectMouseWheel(float xOffset, float yOffset) {
    LAMBUI_LOGT(TAG, "InjectMouseWheel({}, {})", xOffset, yOffset);
    ResetTooltip();
    UIWidget* target = m_activePopup ? HitTestRecursive(*m_activePopup, m_mouseX, m_mouseY) : HitTestInput(m_mouseX, m_mouseY);
    for (UIWidget* w = target; w; w = w->GetParent()) {
        if (auto* scrollable = dynamic_cast<IScrollable*>(w)) {
            scrollable->OnScroll(xOffset, yOffset);
            break;
        }
    }
}

void UIManager::SetFocusedWidget(UIWidget* widget) {
    if (widget == m_focusedWidget) return;
    LAMBUI_LOGT(TAG, "focus '{}' -> '{}'", m_focusedWidget ? m_focusedWidget->GetName() : "<none>",
                widget ? widget->GetName() : "<none>");
    if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) focusable->OnFocusLost();
    m_focusedWidget = widget;
    if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) focusable->OnFocusGained();
    if (!widget) return;
    m_root->ResolveLayout();
    m_overlayRoot->ResolveLayout();
    for (auto* ancestor = widget->GetParent(); ancestor; ancestor = ancestor->GetParent()) {
        if (auto* scroll = dynamic_cast<UIScrollContainer*>(ancestor)) {
            if (!IsWithin(widget, scroll->GetContent())) continue;
            scroll->EnsureVisible(widget->GetComputedRect());
            m_root->ResolveLayout();
            m_overlayRoot->ResolveLayout();
        }
    }
}

void UIManager::CollectFocusTargets(UIWidget& widget, std::vector<UIWidget*>& targets) const {
    if (!widget.IsVisible()) return;
    if (CanFocusWidget(&widget)) targets.push_back(&widget);
    for (const auto& child : widget.GetChildren()) CollectFocusTargets(*child, targets);
}

void UIManager::MoveFocus(bool backwards) {
    LAMBUI_LOGT(TAG, "MoveFocus(backwards={})", backwards);
    std::vector<UIWidget*> targets;
    CollectFocusTargets(m_activePopup ? *m_activePopup : *m_root, targets);
    if (targets.empty()) {
        SetFocusedWidget(nullptr);
        return;
    }
    const auto current = std::find(targets.begin(), targets.end(), m_focusedWidget);
    size_t index = backwards ? targets.size() - 1 : 0;
    if (current != targets.end()) {
        index = static_cast<size_t>(current - targets.begin());
        index = backwards ? (index + targets.size() - 1) % targets.size() : (index + 1) % targets.size();
    }
    SetFocusedWidget(targets[index]);
}

void UIManager::InjectKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "InjectKeyEvent({}, isDown={})", scanCode, isDown);
    ResetTooltip();
    if (scanCode == ScanCode::LeftShift || scanCode == ScanCode::RightShift) {
        (scanCode == ScanCode::LeftShift ? m_leftShift : m_rightShift) = isDown;
        return;
    }
    if (!CanFocusWidget(m_focusedWidget)) SetFocusedWidget(nullptr);
    if (scanCode == ScanCode::Tab) {
        if (isDown) MoveFocus(m_leftShift || m_rightShift);
        return;
    }
    if (m_activePopup) {
        if (scanCode == ScanCode::Escape) {
            if (isDown) ClosePopup();
            return;
        }
        if (dynamic_cast<UIContextMenu*>(m_activePopup)) {
            if (scanCode == ScanCode::Up || scanCode == ScanCode::Down || scanCode == ScanCode::Home || scanCode == ScanCode::End) {
                if (isDown) {
                    if (scanCode == ScanCode::Home || scanCode == ScanCode::End) SetFocusedWidget(nullptr);
                    MoveFocus(scanCode == ScanCode::Up || scanCode == ScanCode::End);
                }
                return;
            }
            if (m_popupAllowsOwnerInput && (scanCode == ScanCode::Left || scanCode == ScanCode::Right)) {
                if (auto* owner = dynamic_cast<IFocusable*>(m_popupOwner)) owner->OnKeyEvent(scanCode, isDown);
                return;
            }
        }
        if (!IsWithin(m_focusedWidget, m_activePopup)) return;
    }
    if (!IsEffectivelyVisible(m_focusedWidget)) return;
    if (isDown) {
        if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
            if (auto* neighbor = focusable->GetFocusNeighbor(scanCode); CanFocusWidget(neighbor) &&
                IsWithin(neighbor, m_activePopup ? m_activePopup : m_root.get())) SetFocusedWidget(neighbor);
        }
    }
    if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
        focusable->OnKeyEvent(scanCode, isDown);
    }
}

void UIManager::InjectCharacter(char32_t codepoint) {
    LAMBUI_LOGT(TAG, "InjectCharacter(U+{:04X})", static_cast<uint32_t>(codepoint));
    ResetTooltip();
    if (!CanFocusWidget(m_focusedWidget)) SetFocusedWidget(nullptr);
    if ((m_activePopup && !IsWithin(m_focusedWidget, m_activePopup)) || !m_focusedWidget) return;
    if (auto* focusable = dynamic_cast<IFocusable*>(m_focusedWidget)) {
        focusable->OnCharacter(codepoint);
    }
}

void UIManager::Update(float deltaTime) {
    LAMBUI_LOGT(TAG, "Update(deltaTime={})", deltaTime);
    if (!CanFocusWidget(m_focusedWidget)) SetFocusedWidget(nullptr);
    m_root->ResolveLayout();
    if (m_activePopup && (!m_activePopup->IsVisible() || (m_popupOwner && !IsEffectivelyVisible(m_popupOwner)))) ClosePopup();
    m_overlayRoot->ResolveLayout();
    if (m_activePopup) {
        PlaceOverlay(*m_activePopup, m_popupX, m_popupY, m_popupWidth, m_popupHeight);
    }
    if (!m_pressedWidget) UpdateHover(m_mouseX, m_mouseY);
    UpdateTooltip(deltaTime);
}

void UIManager::Render() {
    LAMBUI_LOGT(TAG, "Render");
    m_commandBucket.clear();
    m_root->GenerateRenderCommands(m_commandBucket);
    if (m_activePopup) m_activePopup->GenerateRenderCommands(m_commandBucket);
    m_tooltip->GenerateRenderCommands(m_commandBucket);
    if (m_renderer) {
        m_renderer->SubmitRenderCommands(m_commandBucket);
    }
}

void UIManager::SubscribeGameEvent(const std::string& eventName, UIWidget* listener, UIGameEventCallback callback) {
    LAMBUI_LOGT(TAG, "SubscribeGameEvent('{}', listener='{}')", eventName, listener ? listener->GetName() : "<none>");
    m_gameEventListeners.emplace(eventName, GameEventSubscription{listener, std::move(callback)});
}

void UIManager::UnsubscribeGameEvent(const std::string& eventName, UIWidget* listener) {
    LAMBUI_LOGT(TAG, "UnsubscribeGameEvent('{}', listener='{}')", eventName, listener ? listener->GetName() : "<none>");
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
    LAMBUI_LOGT(TAG, "FireGameEvent('{}', payload={})", eventName, fmt::ptr(payload));
    auto range = m_gameEventListeners.equal_range(eventName);
    for (auto it = range.first; it != range.second; ++it) {
        it->second.callback(payload);
    }
}

} // namespace LambUI
