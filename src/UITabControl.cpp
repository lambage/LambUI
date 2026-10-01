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
    if (m_tabs.empty()) return;
    const float width = background.width / static_cast<float>(m_tabs.size());
    for (size_t index = 0; index < m_tabs.size(); ++index) {
        UIRenderCommand header;
        header.x = rect.x + static_cast<float>(index) * width;
        header.y = rect.y;
        header.width = width;
        header.height = std::min(HeaderHeight, background.height);
        header.color = static_cast<int>(index) == m_selectedIndex ? 0x707070FFu : 0x404040FFu;
        bucket.push_back(header);
        header.type = RenderCommandType::PushScissor;
        bucket.push_back(header);
        header.type = RenderCommandType::DrawString;
        header.x += 6.0f;
        header.y += 4.0f;
        header.color = 0xFFFFFFFFu;
        header.text = m_tabs[index].label;
        bucket.push_back(header);
        UIRenderCommand pop;
        pop.type = RenderCommandType::PopScissor;
        bucket.push_back(pop);
    }
}

void UITabControl::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!IsVisible()) return;
    const auto& rect = GetComputedRect();
    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x;
    clip.y = rect.y;
    clip.width = std::max(0.0f, rect.width);
    clip.height = std::max(0.0f, rect.height);
    bucket.push_back(clip);
    UIWidget::GenerateRenderCommands(bucket);
    clip.type = RenderCommandType::PopScissor;
    bucket.push_back(clip);
}

} // namespace LambUI