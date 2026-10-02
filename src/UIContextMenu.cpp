#include "LambUI/UIContextMenu.h"
#include "LambUI/UIManager.h"
#include "LambUI/UIButton.h"
#include "LambUI/UITextWidget.h"
#include "LambUI/UITextureWidget.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace { constexpr const char* TAG = "UIContextMenu"; }

constexpr float UIContextMenu::RowHeight;

UIContextMenu::UIContextMenu(UIManager& manager, std::string name)
    : UIScrollContainer(std::move(name)), m_manager(manager) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    SetVisible(false);
}

void UIContextMenu::SetItems(std::vector<UIMenuItem> items) {
    LAMBUI_LOGT(TAG, "'{}' SetItems({})", GetName(), items.size());
    Close();
    m_items = std::move(items);
    while (m_rows.size() < m_items.size()) {
        const size_t index = m_rows.size();
        auto* button = GetContent()->CreateChild<UIButton>(GetName() + "_Item" + std::to_string(index));
        button->SetKeyboardFocusColor(0x606060FFu);
        button->SetPoint(AnchorPoint::TopLeft, GetContent(), AnchorPoint::TopLeft, 0.0f, static_cast<float>(index) * RowHeight);
        button->RegisterCallback(UIEventType::OnClick, [this, index](const UIEventData& data) {
            LAMBUI_LOGT(TAG, "'{}' item click {}", GetName(), index);
            data.handled = true;
            if (data.button == MouseButton::Left) Activate(index);
        });
        auto* label = button->CreateChild<UITextWidget>();
        label->SetMouseEnabled(false);
        label->SetPoint(AnchorPoint::TopLeft, button, AnchorPoint::TopLeft, 6.0f, 3.0f);
        auto* separator = button->CreateChild<UITextureWidget>();
        separator->SetMouseEnabled(false);
        separator->SetTint(0x606060FFu);
        separator->SetPoint(AnchorPoint::Left, button, AnchorPoint::Left, 6.0f, 0.0f);
        auto* arrow = button->CreateChild<UITextWidget>();
        arrow->SetMouseEnabled(false);
        arrow->SetText(">");
        arrow->SetPoint(AnchorPoint::TopLeft, button, AnchorPoint::TopRight, -24.0f, 3.0f);
        m_rows.push_back({button, label, separator, arrow});
    }
    for (size_t index = 0; index < m_rows.size(); ++index) {
        auto& row = m_rows[index];
        const bool visible = index < m_items.size();
        row.button->SetVisible(visible);
        if (!visible) continue;
        const auto& item = m_items[index];
        row.button->SetMouseEnabled(item.enabled && !item.separator);
        row.button->SetNormalColor(0x303030FFu);
        row.button->SetHoverColor(0x606060FFu);
        row.button->SetPressedColor(0x505050FFu);
        row.label->SetText(item.label);
        row.label->SetFont(m_fontHandle);
        row.label->SetColor(item.enabled ? 0xFFFFFFFFu : 0x909090FFu);
        row.label->SetVisible(!item.separator);
        row.separator->SetVisible(item.separator);
        row.arrow->SetVisible(!item.separator && !item.children.empty());
        row.arrow->SetFont(m_fontHandle);
        row.arrow->SetColor(item.enabled ? 0xFFFFFFFFu : 0x909090FFu);
    }
    SetScrollOffset(0.0f, 0.0f);
}

void UIContextMenu::SetFont(void* fontHandle) {
    m_fontHandle = fontHandle;
    for (const auto& row : m_rows) {
        row.label->SetFont(fontHandle);
        row.arrow->SetFont(fontHandle);
    }
    if (m_submenu) m_submenu->SetFont(fontHandle);
    MarkDirty();
}

void UIContextMenu::Open(float x, float y, UIWidget* owner, bool allowOwnerInput) {
    LAMBUI_LOGT(TAG, "'{}' Open({}, {})", GetName(), x, y);
    if (m_items.empty()) return;
    SizeToItems();
    m_manager.ShowPopup(*this, x, y, owner, allowOwnerInput);
}

void UIContextMenu::SizeToItems() {
    LAMBUI_LOGT(TAG, "'{}' SizeToItems", GetName());
    float width = 160.0f;
    for (const auto& item : m_items) {
        float textWidth = static_cast<float>(item.label.size()) * 8.0f;
        float textHeight = 0.0f;
        if (const auto* measurer = m_manager.GetTextMeasurer()) {
            measurer->MeasureText(item.label, m_fontHandle, textWidth, textHeight);
        }
        if (std::isfinite(textWidth)) width = std::max(width, textWidth + (item.children.empty() ? 12.0f : 36.0f));
    }
    SetSize(width, static_cast<float>(m_items.size()) * RowHeight);
}

void UIContextMenu::Close() {
    LAMBUI_LOGT(TAG, "'{}' Close", GetName());
    if (IsOpen()) m_manager.ClosePopup();
}

bool UIContextMenu::IsOpen() const { return m_manager.IsPopupOpen(this); }

void UIContextMenu::OpenSubmenu(size_t index, bool focus) {
    LAMBUI_LOGT(TAG, "'{}' OpenSubmenu({}, focus={})", GetName(), index, focus);
    if (!IsOpen() || index >= m_items.size()) return;
    const auto& item = m_items[index];
    if (!item.enabled || item.separator || item.children.empty()) {
        m_manager.CloseSubmenus(*this);
        return;
    }
    if (!m_submenu) m_submenu = m_manager.GetOverlayRoot().CreateChild<UIContextMenu>(m_manager, GetName() + "_Submenu");
    if (!m_submenu->IsOpen() || m_submenuIndex != index) {
        m_manager.CloseSubmenus(*this);
        m_submenu->SetItems(item.children);
        m_submenu->SetFont(m_fontHandle);
        m_submenu->SizeToItems();
        m_submenuIndex = index;
    }
    m_manager.ShowSubmenu(*m_submenu, *this, *m_rows[index].button, focus);
}

bool UIContextMenu::OpenFocusedSubmenu() {
    LAMBUI_LOGT(TAG, "'{}' OpenFocusedSubmenu", GetName());
    for (size_t index = 0; index < m_items.size(); ++index) {
        if (m_rows[index].button == m_manager.GetFocusedWidget() && m_items[index].enabled &&
            !m_items[index].separator && !m_items[index].children.empty()) {
            OpenSubmenu(index, true);
            return true;
        }
    }
    return false;
}

void UIContextMenu::HoverRow(float x, float y) {
    LAMBUI_LOGT(TAG, "'{}' HoverRow({}, {})", GetName(), x, y);
    for (size_t index = 0; index < m_items.size(); ++index) {
        const auto& rect = m_rows[index].button->GetComputedRect();
        if (x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height) {
            m_manager.SetFocusedWidget(m_items[index].enabled && !m_items[index].separator ? m_rows[index].button : nullptr);
            OpenSubmenu(index, false);
            return;
        }
    }
}

void UIContextMenu::Activate(size_t index) {
    if (index >= m_items.size() || !m_items[index].enabled || m_items[index].separator) return;
    LAMBUI_LOGT(TAG, "'{}' Activate({})", GetName(), index);
    if (!m_items[index].children.empty()) {
        OpenSubmenu(index, true);
        return;
    }
    auto action = m_items[index].action;
    Close();
    if (action) action();
}

void UIContextMenu::OnLayoutChanged() {
    LAMBUI_LOGT(TAG, "'{}' OnLayoutChanged", GetName());
    UIScrollContainer::OnLayoutChanged();
    const float width = std::max(0.0f, GetComputedRect().width);
    SetContentSize(width, static_cast<float>(m_items.size()) * RowHeight);
    for (const auto& row : m_rows) {
        row.button->SetSize(width, RowHeight);
        row.separator->SetSize(std::max(0.0f, width - 12.0f), 1.0f);
    }
}

void UIContextMenu::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    if (data.type == UIEventType::OnClick || data.type == UIEventType::OnMouseDown || data.type == UIEventType::OnMouseUp) {
        data.handled = true;
    }
}

void UIContextMenu::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    UIRenderCommand background;
    background.x = rect.x;
    background.y = rect.y;
    background.width = rect.width;
    background.height = rect.height;
    background.color = 0x303030FFu;
    bucket.push_back(background);
}

} // namespace LambUI