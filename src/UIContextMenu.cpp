#include "LambUI/UIContextMenu.h"
#include "LambUI/UIManager.h"
#include "LambUI/UIButton.h"
#include "LambUI/UITextWidget.h"
#include "LambUI/UITextureWidget.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace { constexpr const char* TAG = "UIContextMenu"; }

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
        m_rows.push_back({button, label, separator});
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
        row.label->SetColor(item.enabled ? 0xFFFFFFFFu : 0x909090FFu);
        row.label->SetVisible(!item.separator);
        row.separator->SetVisible(item.separator);
    }
    SetScrollOffset(0.0f, 0.0f);
}

void UIContextMenu::Open(float x, float y, UIWidget* owner, bool allowOwnerInput) {
    LAMBUI_LOGT(TAG, "'{}' Open({}, {})", GetName(), x, y);
    if (m_items.empty()) return;
    float width = 160.0f;
    for (const auto& item : m_items) {
        float textWidth = static_cast<float>(item.label.size()) * 8.0f;
        float textHeight = 0.0f;
        if (const auto* measurer = m_manager.GetTextMeasurer()) {
            measurer->MeasureText(item.label, nullptr, textWidth, textHeight);
        }
        if (std::isfinite(textWidth)) width = std::max(width, textWidth + 12.0f);
    }
    SetSize(width, static_cast<float>(m_items.size()) * RowHeight);
    m_manager.ShowPopup(*this, x, y, owner, allowOwnerInput);
}

void UIContextMenu::Close() {
    LAMBUI_LOGT(TAG, "'{}' Close", GetName());
    if (IsOpen()) m_manager.ClosePopup();
}

bool UIContextMenu::IsOpen() const { return m_manager.GetActivePopup() == this; }

void UIContextMenu::Activate(size_t index) {
    if (index >= m_items.size() || !m_items[index].enabled || m_items[index].separator) return;
    LAMBUI_LOGT(TAG, "'{}' Activate({})", GetName(), index);
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