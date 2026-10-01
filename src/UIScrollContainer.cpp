#include "LambUI/UIScrollContainer.h"

#include <algorithm>

namespace LambUI {

namespace {
constexpr const char* TAG = "UIScrollContainer";
} // namespace

class UIScrollContainer::ScrollBar : public UIWidget, public IDraggable {
public:
    ScrollBar(std::string name, UIScrollContainer& owner, bool vertical)
        : UIWidget(std::move(name)), m_owner(owner), m_vertical(vertical) {
        LAMBUI_LOGT(TAG, "scrollbar '{}' constructed (vertical={})", GetName(), vertical);
    }

    ~ScrollBar() override {
        LAMBUI_LOGT(TAG, "scrollbar '{}' destroyed", GetName());
    }

    void Configure(bool visible, float x, float y, float width, float height) {
        LAMBUI_LOGT(TAG, "scrollbar '{}' Configure({}, {}, {}, {}, {})", GetName(), visible, x, y, width, height);
        if (!visible) {
            m_dragging = false;
            m_hovered = false;
        }
        SetVisible(visible);
        SetSize(width, height);
        ClearPoints();
        SetPoint(AnchorPoint::TopLeft, &m_owner, AnchorPoint::TopLeft, x, y);
    }

    void OnDrag(float mouseX, float mouseY) override {
        LAMBUI_LOGT(TAG, "scrollbar '{}' OnDrag({}, {})", GetName(), mouseX, mouseY);
        if (!m_dragging || !IsVisible()) return;
        const auto& rect = GetComputedRect();
        const auto thumb = GetThumbRect();
        const float travel = m_vertical ? rect.height - thumb.height : rect.width - thumb.width;
        if (travel <= 0.0f) return;
        const float position = m_vertical ? mouseY - rect.y : mouseX - rect.x;
        SetOffset((position - m_grabOffset) / travel * GetOverflow());
    }

protected:
    void OnEvent(const UIEventData& data) override {
        LAMBUI_LOGT(TAG, "scrollbar '{}' OnEvent({})", GetName(), ToString(data.type));
        if (data.type == UIEventType::OnMouseEnter) m_hovered = true;
        if (data.type == UIEventType::OnMouseLeave) m_hovered = false;
        if (data.type == UIEventType::OnMouseDown || data.type == UIEventType::OnMouseUp ||
            data.type == UIEventType::OnClick) data.handled = true;
        if (data.button != MouseButton::Left) return;
        if (data.type == UIEventType::OnMouseUp) m_dragging = false;
        if (data.type != UIEventType::OnMouseDown) return;
        const auto thumb = GetThumbRect();
        const float pointer = m_vertical ? data.mouseY : data.mouseX;
        const float start = m_vertical ? thumb.y : thumb.x;
        const float length = m_vertical ? thumb.height : thumb.width;
        m_dragging = pointer >= start && pointer < start + length;
        if (m_dragging) {
            m_grabOffset = pointer - start;
        } else {
            SetOffset(GetOffset() + (pointer < start ? -GetViewportLength() : GetViewportLength()));
        }
    }

    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override {
        const auto& rect = GetComputedRect();
        UIRenderCommand command;
        command.type = RenderCommandType::DrawQuad;
        command.x = rect.x;
        command.y = rect.y;
        command.width = rect.width;
        command.height = rect.height;
        command.color = 0x20282FFFu;
        bucket.push_back(command);
        const auto thumb = GetThumbRect();
        command.x = thumb.x;
        command.y = thumb.y;
        command.width = thumb.width;
        command.height = thumb.height;
        command.color = m_dragging ? 0xD9E5ECFFu : (m_hovered ? 0xA9BDC9FFu : 0x718A99FFu);
        bucket.push_back(command);
    }

private:
    float GetViewportLength() const {
        const auto& rect = m_owner.GetComputedRect();
        return m_vertical ? rect.height : rect.width;
    }

    float GetContentLength() const {
        return m_vertical ? m_owner.m_contentHeight : m_owner.m_contentWidth;
    }

    float GetOverflow() const { return std::max(0.0f, GetContentLength() - GetViewportLength()); }
    float GetOffset() const { return m_vertical ? m_owner.GetScrollY() : m_owner.GetScrollX(); }

    void SetOffset(float offset) {
        LAMBUI_LOGT(TAG, "scrollbar '{}' SetOffset({})", GetName(), offset);
        m_owner.SetScrollOffset(m_vertical ? m_owner.GetScrollX() : offset,
                                m_vertical ? offset : m_owner.GetScrollY());
    }

    UIRect GetThumbRect() const {
        UIRect thumb = GetComputedRect();
        const float length = std::max(0.0f, m_vertical ? thumb.height : thumb.width);
        const float ratio = GetContentLength() > 0.0f ? GetViewportLength() / GetContentLength() : 1.0f;
        const float thumbLength = std::clamp(length * ratio, std::min(20.0f, length), length);
        const float position = GetOverflow() > 0.0f ? GetOffset() / GetOverflow() * (length - thumbLength) : 0.0f;
        if (m_vertical) {
            thumb.y += position;
            thumb.height = thumbLength;
        } else {
            thumb.x += position;
            thumb.width = thumbLength;
        }
        return thumb;
    }

    UIScrollContainer& m_owner;
    bool m_vertical;
    bool m_dragging = false;
    bool m_hovered = false;
    float m_grabOffset = 0.0f;
};

UIScrollContainer::UIScrollContainer(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
    m_content = CreateChild<UIWidget>(GetName() + "_Content");
    m_content->SetPoint(AnchorPoint::TopLeft, this, AnchorPoint::TopLeft, 0.0f, 0.0f);
    m_horizontalBar = CreateChild<ScrollBar>(GetName() + "_HorizontalScrollbar", *this, false);
    m_verticalBar = CreateChild<ScrollBar>(GetName() + "_VerticalScrollbar", *this, true);
    UpdateScrollbars();
}

void UIScrollContainer::SetContentSize(float width, float height) {
    LAMBUI_LOGT(TAG, "'{}' SetContentSize({}, {})", GetName(), width, height);
    m_contentWidth = width;
    m_contentHeight = height;
    m_content->SetSize(width, height);
    ApplyScrollOffset();
    UpdateScrollbars();
}

void UIScrollContainer::SetScrollbarsEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetScrollbarsEnabled({})", GetName(), enabled);
    if (m_scrollbarsEnabled == enabled) return;
    m_scrollbarsEnabled = enabled;
    UpdateScrollbars();
}

void UIScrollContainer::UpdateScrollbars() {
    LAMBUI_LOGT(TAG, "'{}' UpdateScrollbars", GetName());
    const auto& rect = GetComputedRect();
    const float width = std::max(0.0f, rect.width);
    const float height = std::max(0.0f, rect.height);
    const float thickness = std::min({12.0f, width, height});
    const bool horizontal = m_scrollbarsEnabled && thickness > 0.0f && m_contentWidth > width;
    const bool vertical = m_scrollbarsEnabled && thickness > 0.0f && m_contentHeight > height;
    m_horizontalBar->Configure(horizontal, 0.0f, height - thickness,
                               width - (vertical ? thickness : 0.0f), thickness);
    m_verticalBar->Configure(vertical, width - thickness, 0.0f,
                             thickness, height - (horizontal ? thickness : 0.0f));
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
    UpdateScrollbars();
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
