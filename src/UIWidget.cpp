#include "LambUI/UIWidget.h"

namespace LambUI {

namespace {
constexpr const char* TAG = "UIWidget";
} // namespace

UIWidget::UIWidget(std::string name) : m_name(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", m_name);
}

UIWidget::~UIWidget() {
    LAMBUI_LOGT(TAG, "destroying '{}'", m_name);
}

void UIWidget::SetPoint(AnchorPoint myPoint, UIWidget* relativeTo, AnchorPoint relativePoint,
                        float xOffset, float yOffset) {
    UIWidget* resolvedRelativeTo = relativeTo ? relativeTo : m_parent;
    LAMBUI_LOGT(TAG, "'{}' SetPoint({}, relativeTo='{}', {}, offset=({}, {}))", m_name,
                ToString(myPoint), resolvedRelativeTo ? resolvedRelativeTo->GetName() : "<none>",
                ToString(relativePoint), xOffset, yOffset);
    m_anchors.push_back({myPoint, resolvedRelativeTo, relativePoint, xOffset, yOffset});
    MarkDirty();
}

void UIWidget::SetAllPoints(UIWidget* relativeTo) {
    LAMBUI_LOGT(TAG, "'{}' SetAllPoints(relativeTo='{}')", m_name, relativeTo ? relativeTo->GetName() : "<parent>");
    ClearPoints();
    SetPoint(AnchorPoint::TopLeft, relativeTo, AnchorPoint::TopLeft, 0.0f, 0.0f);
    SetPoint(AnchorPoint::BottomRight, relativeTo, AnchorPoint::BottomRight, 0.0f, 0.0f);
}

void UIWidget::ClearPoints() {
    LAMBUI_LOGT(TAG, "'{}' ClearPoints", m_name);
    m_anchors.clear();
    MarkDirty();
}

void UIWidget::SetSize(float width, float height) {
    LAMBUI_LOGT(TAG, "'{}' SetSize({}, {})", m_name, width, height);
    m_width = width;
    m_height = height;
    MarkDirty();
}

void UIWidget::SetVisible(bool visible) {
    if (visible != m_isVisible) {
        LAMBUI_LOGT(TAG, "'{}' SetVisible({})", m_name, visible);
    }
    m_isVisible = visible;
}

void UIWidget::RegisterCallback(UIEventType type, UIEventCallback callback) {
    LAMBUI_LOGT(TAG, "'{}' RegisterCallback({})", m_name, ToString(type));
    m_callbacks[type] = std::move(callback);
}

void UIWidget::FireEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' FireEvent({})", m_name, ToString(data.type));
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
    LAMBUI_LOGT(TAG, "'{}' SetComputedRectDirect(x={}, y={}, w={}, h={})", m_name,
                rect.x, rect.y, rect.width, rect.height);
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
            LAMBUI_LOGT(TAG, "'{}' rect ({}, {}, {}, {}) -> ({}, {}, {}, {})", m_name,
                        previous.x, previous.y, previous.width, previous.height,
                        m_computedRect.x, m_computedRect.y, m_computedRect.width, m_computedRect.height);
            for (auto& child : m_children) child->MarkDirty();
            OnLayoutChanged();
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
