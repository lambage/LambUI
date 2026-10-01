#include "LambUI/UIWindow.h"
#include "LambUI/UIScrollContainer.h"
#include <algorithm>
#include <cmath>

namespace LambUI {
namespace {
constexpr const char* TAG = "UIWindow";
bool Contains(const UIRect& rect, float x, float y) {
    return rect.width > 0.0f && rect.height > 0.0f && x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}
}

UIWindow::UIWindow(std::string name) : UIWidget(std::move(name)), m_title(GetName()) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    SetSize(320.0f, 240.0f);
    m_client = CreateChild<UIScrollContainer>(GetName() + "_Client");
    m_client->SetPoint(AnchorPoint::TopLeft, this, AnchorPoint::TopLeft, Border, TitleHeight);
}
UIWindow::~UIWindow() { LAMBUI_LOGT(TAG, "destroyed '{}'", GetName()); }
UIWidget* UIWindow::GetContent() const { return m_client->GetContent(); }

void UIWindow::SetTitle(std::string title) {
    LAMBUI_LOGT(TAG, "'{}' SetTitle('{}')", GetName(), title);
    m_title = std::move(title);
}
void UIWindow::CancelDrag() {
    LAMBUI_LOGT(TAG, "'{}' CancelDrag", GetName());
    m_dragging = false;
    m_resizeEdges = 0;
    m_pressedButton = -1;
}
void UIWindow::SetMovable(bool movable) {
    LAMBUI_LOGT(TAG, "'{}' SetMovable({})", GetName(), movable);
    m_movable = movable;
    CancelDrag();
}
void UIWindow::SetResizable(bool resizable) {
    LAMBUI_LOGT(TAG, "'{}' SetResizable({})", GetName(), resizable);
    m_resizable = resizable;
    CancelDrag();
}
void UIWindow::SetButtonMode(WindowButton button, WindowButtonMode mode) {
    LAMBUI_LOGT(TAG, "'{}' SetButtonMode({}, {})", GetName(), ToString(button), ToString(mode));
    const auto index = static_cast<size_t>(button);
    if (index >= m_buttons.size()) return;
    m_buttons[index] = mode;
    CancelDrag();
}
WindowButtonMode UIWindow::GetButtonMode(WindowButton button) const {
    const auto index = static_cast<size_t>(button);
    return index < m_buttons.size() ? m_buttons[index] : WindowButtonMode::Hidden;
}
UIRect UIWindow::GetButtonRect(WindowButton button) const {
    if (GetButtonMode(button) == WindowButtonMode::Hidden) return {};
    float right = GetComputedRect().x + GetComputedRect().width - Border;
    for (int index = 2; index >= 0; --index) {
        if (m_buttons[static_cast<size_t>(index)] == WindowButtonMode::Hidden) continue;
        right -= 28.0f;
        if (index == static_cast<int>(button)) return {right, GetComputedRect().y + 4.0f, 26.0f, 24.0f};
    }
    return {};
}
int UIWindow::ButtonAt(float x, float y) const {
    for (int index = 0; index < 3; ++index) if (Contains(GetButtonRect(static_cast<WindowButton>(index)), x, y)) return index;
    return -1;
}
void UIWindow::SetSizeLimits(float minWidth, float minHeight, float maxWidth, float maxHeight) {
    LAMBUI_LOGT(TAG, "'{}' SetSizeLimits({}, {}, {}, {})", GetName(), minWidth, minHeight, maxWidth, maxHeight);
    if (!std::isfinite(minWidth) || !std::isfinite(minHeight) || !std::isfinite(maxWidth) || !std::isfinite(maxHeight)) return;
    m_minWidth = std::max(120.0f, minWidth);
    m_minHeight = std::max(TitleHeight + Border, minHeight);
    m_maxWidth = std::max(m_minWidth, maxWidth);
    m_maxHeight = std::max(m_minHeight, maxHeight);
    if (m_state == WindowState::Normal) {
        SaveNormalBounds();
        SetBounds(m_normalBounds.x, m_normalBounds.y, m_normalBounds.width, m_normalBounds.height);
    }
}
void UIWindow::ApplyBounds(const UIRect& bounds) {
    LAMBUI_LOGT(TAG, "'{}' ApplyBounds({}, {}, {}, {})", GetName(), bounds.x, bounds.y, bounds.width, bounds.height);
    m_boundsPending = true;
    ClearPoints();
    SetPoint(AnchorPoint::TopLeft, GetParent(), AnchorPoint::TopLeft, bounds.x, bounds.y);
    SetSize(bounds.width, bounds.height);
}
void UIWindow::SetBounds(float x, float y, float width, float height) {
    LAMBUI_LOGT(TAG, "'{}' SetBounds({}, {}, {}, {})", GetName(), x, y, width, height);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)) return;
    const bool stateChanged = m_state != WindowState::Normal;
    CancelDrag();
    m_state = WindowState::Normal;
    m_normalBounds = {x, y, std::clamp(width, m_minWidth, m_maxWidth), std::clamp(height, m_minHeight, m_maxHeight)};
    m_client->SetVisible(true);
    ApplyBounds(m_normalBounds);
    if (stateChanged) FireEvent(UIEventData{UIEventType::OnWindowStateChanged});
}
void UIWindow::SaveNormalBounds() {
    LAMBUI_LOGT(TAG, "'{}' SaveNormalBounds", GetName());
    if (m_boundsPending || GetComputedRect().width <= 0.0f) return;
    m_normalBounds = GetComputedRect();
    if (GetParent()) {
        m_normalBounds.x -= GetParent()->GetComputedRect().x;
        m_normalBounds.y -= GetParent()->GetComputedRect().y;
    }
}
void UIWindow::Minimize() {
    LAMBUI_LOGT(TAG, "'{}' Minimize from {}", GetName(), ToString(m_state));
    if (m_state == WindowState::Minimized) return;
    CancelDrag();
    m_restoreMaximized = m_state == WindowState::Maximized;
    if (m_state == WindowState::Normal) SaveNormalBounds();
    m_state = WindowState::Minimized;
    m_client->SetVisible(false);
    auto collapsed = m_normalBounds;
    collapsed.height = TitleHeight;
    ApplyBounds(collapsed);
    FireEvent(UIEventData{UIEventType::OnWindowStateChanged});
}
void UIWindow::Maximize() {
    LAMBUI_LOGT(TAG, "'{}' Maximize from {}", GetName(), ToString(m_state));
    if (!GetParent() || m_state == WindowState::Maximized) return;
    CancelDrag();
    if (m_state == WindowState::Normal) SaveNormalBounds();
    m_state = WindowState::Maximized;
    m_client->SetVisible(true);
    SetAllPoints(GetParent());
    FireEvent(UIEventData{UIEventType::OnWindowStateChanged});
}
void UIWindow::Restore() {
    LAMBUI_LOGT(TAG, "'{}' Restore from {}", GetName(), ToString(m_state));
    if (m_state == WindowState::Normal) return;
    if (m_state == WindowState::Minimized && m_restoreMaximized) { Maximize(); return; }
    CancelDrag();
    m_state = WindowState::Normal;
    m_client->SetVisible(true);
    m_normalBounds.width = std::clamp(m_normalBounds.width, m_minWidth, m_maxWidth);
    m_normalBounds.height = std::clamp(m_normalBounds.height, m_minHeight, m_maxHeight);
    ApplyBounds(m_normalBounds);
    FireEvent(UIEventData{UIEventType::OnWindowStateChanged});
}
void UIWindow::Close() {
    LAMBUI_LOGT(TAG, "'{}' Close", GetName());
    if (!IsVisible()) return;
    CancelDrag();
    SetVisible(false);
    FireEvent(UIEventData{UIEventType::OnClose});
}
void UIWindow::OnPointerActivated() {
    LAMBUI_LOGT(TAG, "'{}' OnPointerActivated", GetName());
    BringToFront();
}
void UIWindow::OnLayoutChanged() {
    LAMBUI_LOGT(TAG, "'{}' OnLayoutChanged", GetName());
    m_boundsPending = false;
    const auto& rect = GetComputedRect();
    const float width = std::max(0.0f, rect.width - Border * 2.0f);
    const float height = std::max(0.0f, rect.height - TitleHeight - Border);
    m_client->SetSize(width, height);
    m_client->SetContentSize(width, height);
}
void UIWindow::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    if (data.type != UIEventType::OnMouseDown && data.type != UIEventType::OnMouseUp && data.type != UIEventType::OnClick) return;
    data.handled = true;
    if (data.button != MouseButton::Left) return;
    const auto rect = GetComputedRect();
    if (data.type == UIEventType::OnMouseDown) {
        CancelDrag();
        m_moved = false;
        m_pressedButton = ButtonAt(data.mouseX, data.mouseY);
        if (m_pressedButton >= 0) return;
        if (m_state == WindowState::Normal && m_resizable) {
            if (data.mouseX < rect.x + Border) m_resizeEdges |= 1;
            if (data.mouseX >= rect.x + rect.width - Border) m_resizeEdges |= 2;
            if (data.mouseY < rect.y + Border) m_resizeEdges |= 4;
            if (data.mouseY >= rect.y + rect.height - Border) m_resizeEdges |= 8;
        }
        m_dragging = m_resizeEdges != 0 || (m_movable && m_state != WindowState::Maximized && data.mouseY < rect.y + TitleHeight);
        m_startX = data.mouseX;
        m_startY = data.mouseY;
        m_dragBounds = rect;
    } else if (data.type == UIEventType::OnMouseUp) {
        m_dragging = false;
        m_resizeEdges = 0;
    } else if (!m_moved && m_pressedButton >= 0 && m_pressedButton == ButtonAt(data.mouseX, data.mouseY)) {
        const auto button = static_cast<WindowButton>(m_pressedButton);
        m_pressedButton = -1;
        if (GetButtonMode(button) != WindowButtonMode::Enabled) return;
        switch (button) {
            case WindowButton::Minimize: if (m_state == WindowState::Minimized) Restore(); else Minimize(); break;
            case WindowButton::Maximize: if (m_state == WindowState::Maximized) Restore(); else Maximize(); break;
            case WindowButton::Close: Close(); break;
        }
    }
}
void UIWindow::OnDrag(float mouseX, float mouseY) {
    if (!m_dragging || !IsVisible() || !std::isfinite(mouseX) || !std::isfinite(mouseY)) return;
    LAMBUI_LOGT(TAG, "'{}' OnDrag({}, {}, edges={})", GetName(), mouseX, mouseY, m_resizeEdges);
    const float deltaX = mouseX - m_startX;
    const float deltaY = mouseY - m_startY;
    if (!m_moved && std::abs(deltaX) + std::abs(deltaY) < 1.0f) return;
    m_moved = true;
    auto bounds = m_dragBounds;
    if (!m_resizeEdges) {
        bounds.x += deltaX;
        bounds.y += deltaY;
    } else {
        if (m_resizeEdges & 1) { bounds.width = std::clamp(m_dragBounds.width - deltaX, m_minWidth, m_maxWidth); bounds.x += m_dragBounds.width - bounds.width; }
        if (m_resizeEdges & 2) bounds.width = std::clamp(m_dragBounds.width + deltaX, m_minWidth, m_maxWidth);
        if (m_resizeEdges & 4) { bounds.height = std::clamp(m_dragBounds.height - deltaY, m_minHeight, m_maxHeight); bounds.y += m_dragBounds.height - bounds.height; }
        if (m_resizeEdges & 8) bounds.height = std::clamp(m_dragBounds.height + deltaY, m_minHeight, m_maxHeight);
    }
    if (GetParent()) {
        bounds.x -= GetParent()->GetComputedRect().x;
        bounds.y -= GetParent()->GetComputedRect().y;
    }
    if (!m_resizeEdges && GetParent()) {
        bounds.x = std::clamp(bounds.x, 0.0f, std::max(0.0f, GetParent()->GetComputedRect().width - bounds.width));
        bounds.y = std::clamp(bounds.y, 0.0f, std::max(0.0f, GetParent()->GetComputedRect().height - TitleHeight));
    }
    if (m_state == WindowState::Minimized) { m_normalBounds.x = bounds.x; m_normalBounds.y = bounds.y; }
    else if (m_state == WindowState::Normal) m_normalBounds = bounds;
    ApplyBounds(bounds);
}
void UIWindow::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    const auto quad = [&](float x, float y, float width, float height, uint32_t color) {
        UIRenderCommand command;
        command.x = x; command.y = y; command.width = width; command.height = height; command.color = color;
        bucket.push_back(command);
    };
    quad(rect.x, rect.y, rect.width, rect.height, 0x45535BFFu);
    quad(rect.x + Border, rect.y + TitleHeight, std::max(0.0f, rect.width - Border * 2), std::max(0.0f, rect.height - TitleHeight - Border), 0x242A2DFFu);
    float titleRight = rect.x + rect.width - Border;
    for (int index = 0; index < 3; ++index) {
        const auto button = static_cast<WindowButton>(index);
        const auto bounds = GetButtonRect(button);
        if (bounds.width == 0.0f) continue;
        titleRight = std::min(titleRight, bounds.x);
        const uint32_t ink = GetButtonMode(button) == WindowButtonMode::Enabled ? 0xFFFFFFFFu : 0x7A858BFFu;
        quad(bounds.x, bounds.y, bounds.width, bounds.height, 0x303A40FFu);
        if (button == WindowButton::Minimize && m_state != WindowState::Minimized) {
            quad(bounds.x + 7, bounds.y + 16, 12, 2, ink);
        } else if (button == WindowButton::Close) {
            for (int step = 0; step < 10; ++step) {
                quad(bounds.x + 8 + step, bounds.y + 7 + step, 2, 2, ink);
                quad(bounds.x + 17 - step, bounds.y + 7 + step, 2, 2, ink);
            }
        } else {
            const bool restore = m_state != WindowState::Normal;
            if (restore) { quad(bounds.x + 10, bounds.y + 6, 11, 2, ink); quad(bounds.x + 19, bounds.y + 6, 2, 9, ink); }
            quad(bounds.x + 6, bounds.y + 9, 12, 2, ink);
            quad(bounds.x + 6, bounds.y + 9, 2, 10, ink);
            quad(bounds.x + 16, bounds.y + 9, 2, 10, ink);
            quad(bounds.x + 6, bounds.y + 17, 12, 2, ink);
        }
    }
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x + Border; clip.y = rect.y; clip.width = std::max(0.0f, titleRight - rect.x - Border * 2); clip.height = TitleHeight;
    bucket.push_back(clip);
    UIRenderCommand title;
    title.type = RenderCommandType::DrawString;
    title.x = rect.x + Border + 2; title.y = rect.y + 5; title.text = m_title;
    bucket.push_back(title);
    clip.type = RenderCommandType::PopScissor;
    bucket.push_back(clip);
    if (m_resizable && m_state == WindowState::Normal) {
        for (int step = 0; step < 3; ++step) quad(rect.x + rect.width - 4 - step * 3, rect.y + rect.height - 3, 2, 2, 0xB9E5D8FFu);
    }
}
void UIWindow::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!IsVisible()) return;
    AppendStyleShadow(bucket);
    const auto& rect = GetComputedRect();
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x; clip.y = rect.y; clip.width = std::max(0.0f, rect.width); clip.height = std::max(0.0f, rect.height);
    bucket.push_back(clip);
    GenerateContentRenderCommands(bucket);
    clip.type = RenderCommandType::PopScissor;
    bucket.push_back(clip);
}
} // namespace LambUI