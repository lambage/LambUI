#include "LambUI/UIMenuBar.h"
#include "LambUI/UIManager.h"
#include "LambUI/UIButton.h"
#include "LambUI/UITextWidget.h"
#include <algorithm>

namespace LambUI {

namespace { constexpr const char* TAG = "UIMenuBar"; }

UIMenuBar::UIMenuBar(UIManager& manager, std::string name)
    : UIControl(std::move(name)), m_manager(manager) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}

UIContextMenu* UIMenuBar::AddMenu(std::string label, std::vector<UIMenuItem> items) {
    LAMBUI_LOGT(TAG, "'{}' AddMenu('{}')", GetName(), label);
    const size_t index = m_entries.size();
    auto* menu = m_manager.GetOverlayRoot().CreateChild<UIContextMenu>(m_manager, GetName() + "_Menu" + std::to_string(index));
    menu->SetItems(std::move(items));
    auto* button = CreateChild<UIButton>(GetName() + "_Header" + std::to_string(index));
    button->SetKeyboardEnabled(false);
    button->SetNormalColor(0x404040FFu);
    button->SetHoverColor(0x606060FFu);
    button->SetPressedColor(0x505050FFu);
    auto* text = button->CreateChild<UITextWidget>();
    text->SetMouseEnabled(false);
    text->SetText(label);
    text->SetPoint(AnchorPoint::TopLeft, button, AnchorPoint::TopLeft, 6.0f, 4.0f);
    button->RegisterCallback(UIEventType::OnClick, [this, index](const UIEventData& data) {
        LAMBUI_LOGT(TAG, "'{}' header click {}", GetName(), index);
        data.handled = true;
        if (data.button != MouseButton::Left) return;
        if (m_entries[index].menu->IsOpen()) m_entries[index].menu->Close();
        else OpenMenu(index);
    });
    button->RegisterCallback(UIEventType::OnMouseEnter, [this, index](const UIEventData&) {
        LAMBUI_LOGT(TAG, "'{}' header enter {}", GetName(), index);
        for (const auto& entry : m_entries) {
            if (entry.menu->IsOpen() && entry.menu != m_entries[index].menu) {
                OpenMenu(index);
                break;
            }
        }
    });
    m_entries.push_back({button, menu});
    OnLayoutChanged();
    return menu;
}

UIContextMenu* UIMenuBar::GetMenu(size_t index) const {
    return index < m_entries.size() ? m_entries[index].menu : nullptr;
}

void UIMenuBar::OpenMenu(size_t index) {
    LAMBUI_LOGT(TAG, "'{}' OpenMenu({})", GetName(), index);
    if (index >= m_entries.size()) return;
    m_selectedIndex = index;
    const auto& entry = m_entries[index];
    const auto& rect = entry.button->GetComputedRect();
    entry.menu->Open(rect.x, rect.y + rect.height, this, true);
    HighlightHeader(true);
}

void UIMenuBar::HighlightHeader(bool highlighted) {
    LAMBUI_LOGT(TAG, "'{}' HighlightHeader({})", GetName(), highlighted);
    for (size_t index = 0; index < m_entries.size(); ++index) {
        m_entries[index].button->SetNormalColor(highlighted && index == m_selectedIndex ? 0x606060FFu : 0x404040FFu);
    }
}

void UIMenuBar::OnFocusGained() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusGained", GetName());
    UIControl::OnFocusGained();
    HighlightHeader(true);
}

void UIMenuBar::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    UIControl::OnFocusLost();
    HighlightHeader(m_selectedIndex < m_entries.size() && m_entries[m_selectedIndex].menu->IsOpen());
}

void UIMenuBar::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown || m_entries.empty()) return;
    const bool open = m_entries[m_selectedIndex].menu->IsOpen();
    if (scanCode == ScanCode::Left || scanCode == ScanCode::Right || scanCode == ScanCode::Home || scanCode == ScanCode::End) {
        if (scanCode == ScanCode::Home) m_selectedIndex = 0;
        else if (scanCode == ScanCode::End) m_selectedIndex = m_entries.size() - 1;
        else m_selectedIndex = (m_selectedIndex + (scanCode == ScanCode::Left ? m_entries.size() - 1 : 1)) % m_entries.size();
        HighlightHeader(true);
        if (open) OpenMenu(m_selectedIndex);
    } else if (scanCode == ScanCode::Down || scanCode == ScanCode::Up || scanCode == ScanCode::Enter || scanCode == ScanCode::Space) {
        OpenMenu(m_selectedIndex);
    }
}

void UIMenuBar::OnLayoutChanged() {
    LAMBUI_LOGT(TAG, "'{}' OnLayoutChanged", GetName());
    if (m_entries.empty()) return;
    const auto& rect = GetComputedRect();
    const float width = std::max(0.0f, rect.width) / static_cast<float>(m_entries.size());
    for (size_t index = 0; index < m_entries.size(); ++index) {
        auto* button = m_entries[index].button;
        button->ClearPoints();
        button->SetPoint(AnchorPoint::TopLeft, this, AnchorPoint::TopLeft, static_cast<float>(index) * width, 0.0f);
        button->SetSize(width, std::max(0.0f, rect.height));
    }
}

void UIMenuBar::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!IsVisible()) return;
    for (const auto& child : GetChildren()) {
        const auto& rect = child->GetComputedRect();
        UIRenderCommand clip;
        clip.type = RenderCommandType::PushScissor;
        clip.x = rect.x;
        clip.y = rect.y;
        clip.width = rect.width;
        clip.height = rect.height;
        bucket.push_back(clip);
        AppendChildRenderCommands(*child, bucket);
        clip.type = RenderCommandType::PopScissor;
        bucket.push_back(clip);
    }
}

} // namespace LambUI