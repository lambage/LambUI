#include "LambUI/UIScrollContainer.h"

#include <algorithm>

namespace LambUI {

namespace {
constexpr const char* TAG = "UIScrollContainer";
} // namespace

UIScrollContainer::UIScrollContainer(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    m_content = CreateChild<UIWidget>(GetName() + "_Content");
    m_content->SetPoint(AnchorPoint::TopLeft, this, AnchorPoint::TopLeft, 0.0f, 0.0f);
}

void UIScrollContainer::SetContentSize(float width, float height) {
    LAMBUI_LOGT(TAG, "'{}' SetContentSize({}, {})", GetName(), width, height);
    m_contentWidth = width;
    m_contentHeight = height;
    m_content->SetSize(width, height);
    ApplyScrollOffset();
}

void UIScrollContainer::SetScrollOffset(float x, float y) {
    LAMBUI_LOGT(TAG, "'{}' SetScrollOffset({}, {})", GetName(), x, y);
    m_scrollX = x;
    m_scrollY = y;
    ApplyScrollOffset();
}

void UIScrollContainer::OnScroll(float xOffset, float yOffset) {
    LAMBUI_LOGT(TAG, "'{}' OnScroll({}, {})", GetName(), xOffset, yOffset);
    SetScrollOffset(m_scrollX - xOffset, m_scrollY - yOffset);
}

void UIScrollContainer::ApplyScrollOffset() {
    const UIRect& rect = GetComputedRect();
    const float maxScrollX = std::max(0.0f, m_contentWidth - rect.width);
    const float maxScrollY = std::max(0.0f, m_contentHeight - rect.height);
    m_scrollX = std::clamp(m_scrollX, 0.0f, maxScrollX);
    m_scrollY = std::clamp(m_scrollY, 0.0f, maxScrollY);

    LAMBUI_LOGT(TAG, "'{}' scroll offset -> ({}, {})", GetName(), m_scrollX, m_scrollY);

    m_content->ClearPoints();
    m_content->SetPoint(AnchorPoint::TopLeft, this, AnchorPoint::TopLeft, -m_scrollX, -m_scrollY);
}

void UIScrollContainer::OnLayoutChanged() {
    LAMBUI_LOGT(TAG, "'{}' OnLayoutChanged", GetName());
    ApplyScrollOffset();
}

void UIScrollContainer::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!IsVisible()) return;

    const UIRect& rect = GetComputedRect();
    UIRenderCommand pushCmd;
    pushCmd.type = RenderCommandType::PushScissor;
    pushCmd.x = rect.x;
    pushCmd.y = rect.y;
    pushCmd.width = rect.width;
    pushCmd.height = rect.height;
    bucket.push_back(pushCmd);

    UIWidget::GenerateRenderCommands(bucket);

    UIRenderCommand popCmd;
    popCmd.type = RenderCommandType::PopScissor;
    bucket.push_back(popCmd);
}

} // namespace LambUI
