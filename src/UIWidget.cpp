#include "LambUI/UIWidget.h"

namespace LambUI {

UIWidget::UIWidget(std::string name) : m_name(std::move(name)) {}

void UIWidget::SetPoint(AnchorPoint myPoint, UIWidget* relativeTo, AnchorPoint relativePoint,
                        float xOffset, float yOffset) {
    m_anchors.push_back({myPoint, relativeTo ? relativeTo : m_parent, relativePoint, xOffset, yOffset});
    MarkDirty();
}

void UIWidget::SetAllPoints(UIWidget* relativeTo) {
    ClearPoints();
    SetPoint(AnchorPoint::TopLeft, relativeTo, AnchorPoint::TopLeft, 0.0f, 0.0f);
    SetPoint(AnchorPoint::BottomRight, relativeTo, AnchorPoint::BottomRight, 0.0f, 0.0f);
}

void UIWidget::ClearPoints() {
    m_anchors.clear();
    MarkDirty();
}

void UIWidget::SetSize(float width, float height) {
    m_width = width;
    m_height = height;
    MarkDirty();
}

void UIWidget::SetVisible(bool visible) {
    m_isVisible = visible;
}

void UIWidget::RegisterCallback(UIEventType type, UIEventCallback callback) {
    m_callbacks[type] = std::move(callback);
}

void UIWidget::FireEvent(const UIEventData& data) {
    OnEvent(data);
    auto it = m_callbacks.find(data.type);
    if (it != m_callbacks.end()) {
        it->second(data);
    }
}

bool UIWidget::HitTest(float x, float y) const {
    if (!m_isVisible || !m_isMouseEnabled) return false;
    return x >= m_computedRect.x && x <= m_computedRect.x + m_computedRect.width &&
           y >= m_computedRect.y && y <= m_computedRect.y + m_computedRect.height;
}

void UIWidget::MarkDirty() {
    m_isDirty = true;
}

void UIWidget::SetComputedRectDirect(const UIRect& rect) {
    m_computedRect = rect;
    m_isDirty = false;
    for (auto& child : m_children) child->MarkDirty();
}

void UIWidget::ResolveLayout() {
    if (m_isDirty) {
        const UIRect previous = m_computedRect;

        std::vector<AnchorConstraint> constraints;
        constraints.reserve(m_anchors.size());
        for (const auto& binding : m_anchors) {
            const UIWidget* relativeTo = binding.relativeTo ? binding.relativeTo : m_parent;
            if (!relativeTo) continue;
            constraints.push_back({binding.myPoint, relativeTo->GetComputedRect(),
                                    binding.relativePoint, binding.xOffset, binding.yOffset});
        }

        m_computedRect = ResolveAnchoredRect(constraints, m_width, m_height, previous);
        m_isDirty = false;

        const bool rectChanged = m_computedRect.x != previous.x || m_computedRect.y != previous.y ||
                                  m_computedRect.width != previous.width ||
                                  m_computedRect.height != previous.height;
        if (rectChanged) {
            for (auto& child : m_children) child->MarkDirty();
        }
    }

    for (auto& child : m_children) child->ResolveLayout();
}

void UIWidget::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!m_isVisible) return;
    OnGenerateRenderCommands(bucket);
    for (auto& child : m_children) child->GenerateRenderCommands(bucket);
}

} // namespace LambUI
