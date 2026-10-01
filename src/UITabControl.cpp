#include "LambUI/UITabControl.h"
#include <algorithm>

namespace LambUI {

namespace { constexpr const char* TAG = "UITabControl"; }

UITabControl::UITabControl(std::string name) : UIControl(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}

UIWidget* UITabControl::AddTab(std::string label) {
    LAMBUI_LOGT(TAG, "'{}' AddTab('{}')", GetName(), label);
    auto* page = CreateChild<UIWidget>(GetName() + "_Page" + std::to_string(m_tabs.size()));
    page->SetPoint(AnchorPoint::TopLeft, this, AnchorPoint::TopLeft, 0.0f, HeaderHeight);
    page->SetVisible(false);
    m_tabs.push_back({std::move(label), page});
    OnLayoutChanged();
    if (m_selectedIndex == -1) SetSelectedIndex(0);
    return page;
}

UIWidget* UITabControl::GetPage(int index) const {
    return index >= 0 && index < static_cast<int>(m_tabs.size()) ? m_tabs[static_cast<size_t>(index)].page : nullptr;
}

void UITabControl::SetTabText(int index, std::string label) {
    if (!GetPage(index)) return;
    LAMBUI_LOGT(TAG, "'{}' SetTabText({}, '{}')", GetName(), index, label);
    m_tabs[static_cast<size_t>(index)].label = std::move(label);
    MarkDirty();
}

void UITabControl::SetSelectedIndex(int index) {
    if (!GetPage(index) || index == m_selectedIndex) return;
    LAMBUI_LOGT(TAG, "'{}' selection {} -> {}", GetName(), m_selectedIndex, index);
    if (auto* previous = GetPage(m_selectedIndex)) previous->SetVisible(false);
    m_selectedIndex = index;
    GetPage(index)->SetVisible(true);
    FireEvent(UIEventData{UIEventType::OnValueChanged});
}

void UITabControl::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown || m_tabs.empty()) return;
    const int count = static_cast<int>(m_tabs.size());
    if (scanCode == ScanCode::Right || scanCode == ScanCode::Down) SetSelectedIndex((m_selectedIndex + 1) % count);
    else if (scanCode == ScanCode::Left || scanCode == ScanCode::Up) SetSelectedIndex((m_selectedIndex + count - 1) % count);
    else if (scanCode == ScanCode::Home) SetSelectedIndex(0);
    else if (scanCode == ScanCode::End) SetSelectedIndex(count - 1);
}

void UITabControl::OnLayoutChanged() {
    LAMBUI_LOGT(TAG, "'{}' OnLayoutChanged", GetName());
    const auto& rect = GetComputedRect();
    for (const auto& tab : m_tabs) {
        tab.page->SetSize(std::max(0.0f, rect.width), std::max(0.0f, rect.height - HeaderHeight));
    }
}

void UITabControl::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    UIControl::OnEvent(data);
    const auto& rect = GetComputedRect();
    if (data.type != UIEventType::OnClick || data.button != MouseButton::Left || m_tabs.empty() ||
        rect.width <= 0.0f || data.mouseX < rect.x || data.mouseX >= rect.x + rect.width ||
        data.mouseY < rect.y || data.mouseY >= rect.y + std::min(HeaderHeight, rect.height)) return;
    data.handled = true;
    SetSelectedIndex(static_cast<int>((data.mouseX - rect.x) / rect.width * static_cast<float>(m_tabs.size())));
}

void UITabControl::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    UIRenderCommand background;
    background.x = rect.x;
    background.y = rect.y;
    background.width = std::max(0.0f, rect.width);
    background.height = std::max(0.0f, rect.height);
    background.color = 0x303030FFu;
    bucket.push_back(background);
    if (m_tabs.empty() || background.width <= 0 || background.height <= 0) return;
    const float width = background.width / static_cast<float>(m_tabs.size());
    const float height = std::min(HeaderHeight, background.height);
    UIRenderCommand strip = background;
    strip.height = height;
    strip.color = 0x20262BFFu;
    bucket.push_back(strip);
    for (size_t index = 0; index < m_tabs.size(); ++index) {
        const bool selected = static_cast<int>(index) == m_selectedIndex;
        const float gap = std::min(1.0f, width * 0.25f);
        const float topInset = selected ? 0.0f : std::min(3.0f, height);
        UIRenderCommand header;
        header.x = rect.x + static_cast<float>(index) * width + gap;
        header.y = rect.y + topInset;
        header.width = std::max(0.0f, width - gap * 2);
        header.height = height - topInset;
        header.color = selected ? 0x4B555DFFu : 0x343D44FFu;
        bucket.push_back(header);
        UIRenderCommand edge = header;
        edge.height = std::min(2.0f, header.height);
        edge.y = selected ? rect.y : rect.y + height - edge.height;
        edge.color = selected ? (HasKeyboardFocus() ? 0xB9F4DDFFu : 0x67DBB3FFu) : 0x71808AFFu;
        bucket.push_back(edge);
        header.type = RenderCommandType::PushScissor;
        header.x += std::min(6.0f, header.width);
        header.width = std::max(0.0f, header.width - 12.0f);
        header.y = rect.y;
        header.height = height;
        bucket.push_back(header);
        header.type = RenderCommandType::DrawString;
        header.y += 4.0f;
        header.color = selected ? 0xFFFFFFFFu : 0xC3CDD4FFu;
        header.text = m_tabs[index].label;
        bucket.push_back(header);
        UIRenderCommand pop;
        pop.type = RenderCommandType::PopScissor;
        bucket.push_back(pop);
    }
}

void UITabControl::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!IsVisible()) return;
    AppendStyleShadow(bucket);
    const auto& rect = GetComputedRect();
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x;
    clip.y = rect.y;
    clip.width = std::max(0.0f, rect.width);
    clip.height = std::max(0.0f, rect.height);
    bucket.push_back(clip);
    GenerateContentRenderCommands(bucket);
    clip.type = RenderCommandType::PopScissor;
    bucket.push_back(clip);
}

} // namespace LambUI