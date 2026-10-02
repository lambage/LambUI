#include "LambUI/UIWidget.h"
#include "LambUI/UIClamp.h"
#include <algorithm>
#include <cmath>

namespace LambUI {

namespace {
constexpr const char* TAG = "UIWidget";

float Nonnegative(float value) {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

UIInsets NormalizeInsets(UIInsets value) {
    return {Nonnegative(value.left), Nonnegative(value.top),
            Nonnegative(value.right), Nonnegative(value.bottom)};
}

UIRect InsetRect(UIRect rect, UIInsets insets) {
    rect.x += std::min(insets.left, rect.width);
    rect.y += std::min(insets.top, rect.height);
    rect.width = std::max(0.0f, rect.width - insets.left - insets.right);
    rect.height = std::max(0.0f, rect.height - insets.top - insets.bottom);
    return rect;
}

UIRect AnchorFractions(AnchorPoint point) {
    return ResolveAnchoredRect({{AnchorPoint::TopLeft, {0, 0, 1, 1}, point}}, 0, 0, {});
}

} // namespace

void UIWidget::DestroyChildren() {
    LAMBUI_LOGT(TAG, "DestroyChildren: {} children removed from '{}'", m_children.size(), m_name);
    m_children.clear();
    MarkDirty();
}

namespace {

void AppendStyledFill(std::vector<UIRenderCommand>& bucket, const UIRenderCommand& source,
                      float radius, UIFillPattern pattern, uint32_t patternColor, float patternSize) {
    if (source.width <= 0 || source.height <= 0) return;
    radius = std::min({radius, source.width * 0.5f, source.height * 0.5f});
    if (radius == 0 && pattern == UIFillPattern::Solid) {
        if ((source.color & 0xFFu) != 0) bucket.push_back(source);
        return;
    }
    const float cell = std::max({1.0f, patternSize, source.width / 64.0f, source.height / 64.0f});
    const float curveStep = std::max(1.0f, radius / 32.0f);
    float rowTop = 0;
    while (rowTop < source.height) {
        float rowBottom = source.height;
        if (rowTop < radius) rowBottom = std::min(radius, rowTop + curveStep);
        else if (rowTop < source.height - radius) rowBottom = source.height - radius;
        else rowBottom = std::min(source.height, rowTop + curveStep);
        const int row = static_cast<int>(rowTop / cell);
        if (pattern != UIFillPattern::Solid) rowBottom = std::min(rowBottom, (row + 1) * cell);
        if (rowBottom <= rowTop) break;
        const float midpoint = (rowTop + rowBottom) * 0.5f;
        const float distance = std::max(0.0f, radius - std::min(midpoint, source.height - midpoint));
        const float inset = radius - std::sqrt(std::max(0.0f, radius * radius - distance * distance));
        float columnLeft = inset;
        while (columnLeft < source.width - inset) {
            const int column = static_cast<int>(columnLeft / cell);
            const float columnRight = pattern == UIFillPattern::Checkerboard
                ? std::min(source.width - inset, (column + 1) * cell) : source.width - inset;
            if (columnRight <= columnLeft) break;
            auto command = source;
            command.x += columnLeft;
            command.y += rowTop;
            command.width = columnRight - columnLeft;
            command.height = rowBottom - rowTop;
            command.u0 = source.u0 + (source.u1 - source.u0) * columnLeft / source.width;
            command.u1 = source.u0 + (source.u1 - source.u0) * columnRight / source.width;
            command.v0 = source.v0 + (source.v1 - source.v0) * rowTop / source.height;
            command.v1 = source.v0 + (source.v1 - source.v0) * rowBottom / source.height;
            if ((pattern == UIFillPattern::Checkerboard && (row + column) % 2 != 0) ||
                (pattern == UIFillPattern::HorizontalStripes && row % 2 != 0)) command.color = patternColor;
            if ((command.color & 0xFFu) != 0) bucket.push_back(std::move(command));
            columnLeft = columnRight;
        }
        rowTop = rowBottom;
    }
}
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
    m_relativeWidth = m_relativeHeight = -1.0f;
    MarkDirty();
}

void UIWidget::SetMargin(UIInsets margin) {
    LAMBUI_LOGT(TAG, "'{}' SetMargin({}, {}, {}, {})", m_name, margin.left, margin.top, margin.right, margin.bottom);
    m_margin = NormalizeInsets(margin);
    MarkDirty();
}

void UIWidget::SetPadding(UIInsets padding) {
    LAMBUI_LOGT(TAG, "'{}' SetPadding({}, {}, {}, {})", m_name, padding.left, padding.top, padding.right, padding.bottom);
    m_padding = NormalizeInsets(padding);
    m_paddingChanged = true;
    MarkDirty();
    for (auto& child : m_children) child->MarkDirty();
}

UIRect UIWidget::GetContentRect() const {
    return InsetRect(m_computedRect, m_padding);
}

void UIWidget::SetMinSize(float width, float height) {
    LAMBUI_LOGT(TAG, "'{}' SetMinSize({}, {})", m_name, width, height);
    m_minWidth = Nonnegative(width);
    m_minHeight = Nonnegative(height);
    MarkDirty();
}

void UIWidget::SetMaxSize(float width, float height) {
    LAMBUI_LOGT(TAG, "'{}' SetMaxSize({}, {})", m_name, width, height);
    m_maxWidth = std::isinf(width) && width > 0 ? width : Nonnegative(width);
    m_maxHeight = std::isinf(height) && height > 0 ? height : Nonnegative(height);
    MarkDirty();
}

void UIWidget::SetAspectRatio(float widthOverHeight) {
    LAMBUI_LOGT(TAG, "'{}' SetAspectRatio({})", m_name, widthOverHeight);
    m_aspectRatio = Nonnegative(widthOverHeight);
    MarkDirty();
}

void UIWidget::SetRelativeSize(float widthFraction, float heightFraction) {
    LAMBUI_LOGT(TAG, "'{}' SetRelativeSize({}, {})", m_name, widthFraction, heightFraction);
    m_relativeWidth = std::isfinite(widthFraction) ? widthFraction : -1.0f;
    m_relativeHeight = std::isfinite(heightFraction) ? heightFraction : -1.0f;
    MarkDirty();
}

void UIWidget::SetStyle(const UIStyle& style) {
    LAMBUI_LOGT(TAG, "'{}' SetStyle(fill={}, radius={}, pattern={}, color={}, size={}, shadow={}, offset=({}, {}), blur={}, spread={})",
                m_name, style.fillColor.value_or(0), style.cornerRadius, ToString(style.pattern), style.patternColor,
                style.patternSize, style.shadowColor, style.shadowOffsetX, style.shadowOffsetY, style.shadowBlur, style.shadowSpread);
    m_style = style;
    m_style->cornerRadius = Nonnegative(style.cornerRadius);
    m_style->patternSize = std::max(1.0f, Nonnegative(style.patternSize));
    m_style->shadowBlur = Nonnegative(style.shadowBlur);
    m_style->shadowSpread = Nonnegative(style.shadowSpread);
    m_style->shadowOffsetX = std::isfinite(style.shadowOffsetX) ? style.shadowOffsetX : 0;
    m_style->shadowOffsetY = std::isfinite(style.shadowOffsetY) ? style.shadowOffsetY : 0;
    MarkDirty();
}

void UIWidget::ClearStyle() {
    LAMBUI_LOGT(TAG, "'{}' ClearStyle", m_name);
    m_style.reset();
    MarkDirty();
}

void UIWidget::SetKeyboardEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetKeyboardEnabled({})", GetName(), enabled);
    m_isKeyboardEnabled = enabled;
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

void UIWidget::SetTooltip(std::string text) {
    LAMBUI_LOGT(TAG, "'{}' SetTooltip('{}')", m_name, text);
    m_tooltip = std::move(text);
}

void UIWidget::FireEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' FireEvent({})", m_name, ToString(data.type));
    const bool bubbles = data.type != UIEventType::OnMouseEnter &&
                         data.type != UIEventType::OnMouseLeave;
    for (UIWidget* widget = this; widget && !data.handled; widget = widget->m_parent) {
        LAMBUI_LOGT(TAG, "'{}' dispatch({})", widget->m_name, ToString(data.type));
        widget->OnEvent(data);
        auto it = widget->m_callbacks.find(data.type);
        if (it != widget->m_callbacks.end() && it->second) {
            it->second(data);
        }
        LAMBUI_LOGT(TAG, "'{}' dispatched({}, handled={})", widget->m_name,
                    ToString(data.type), data.handled);
        if (!bubbles) break;
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

void UIWidget::BringToFront() {
    LAMBUI_LOGT(TAG, "'{}' BringToFront", m_name);
    if (!m_parent) return;
    auto& siblings = m_parent->m_children;
    const auto position = std::find_if(siblings.begin(), siblings.end(), [this](const auto& child) { return child.get() == this; });
    if (position != siblings.end()) std::rotate(position, position + 1, siblings.end());
}

void UIWidget::SetComputedRectDirect(const UIRect& rect) {
    LAMBUI_LOGT(TAG, "'{}' SetComputedRectDirect(x={}, y={}, w={}, h={})", m_name,
                rect.x, rect.y, rect.width, rect.height);
    m_computedRect = rect;
    m_width = rect.width;
    m_height = rect.height;
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
            const auto fraction = AnchorFractions(binding.myPoint);
            const auto target = relativeTo == m_parent ? relativeTo->GetContentRect() : relativeTo->GetComputedRect();
            constraints.push_back({binding.myPoint, target, binding.relativePoint,
                binding.xOffset + m_margin.left * (1.0f - fraction.x) - m_margin.right * fraction.x,
                binding.yOffset + m_margin.top * (1.0f - fraction.y) - m_margin.bottom * fraction.y});
        }

        const auto available = m_parent ? InsetRect(m_parent->GetContentRect(), m_margin) : UIRect{};
        const float width = m_relativeWidth >= 0 && m_parent ? available.width * m_relativeWidth : Nonnegative(m_width);
        const float height = m_relativeHeight >= 0 && m_parent ? available.height * m_relativeHeight : Nonnegative(m_height);
        m_computedRect = ResolveAnchoredRect(constraints, width, height,
                                            {previous.x, previous.y, width, height});
        const float maxWidth = std::max(m_minWidth, m_maxWidth);
        const float maxHeight = std::max(m_minHeight, m_maxHeight);
        float resolvedWidth = Clamp(m_computedRect.width, m_minWidth, maxWidth);
        float resolvedHeight = Clamp(m_computedRect.height, m_minHeight, maxHeight);
        if (m_aspectRatio > 0) {
            const float lower = std::max(m_minWidth, m_minHeight * m_aspectRatio);
            const float upper = std::min(maxWidth, maxHeight * m_aspectRatio);
            if (lower <= upper) {
                resolvedWidth = Clamp(std::min(resolvedWidth, resolvedHeight * m_aspectRatio), lower, upper);
                resolvedHeight = resolvedWidth / m_aspectRatio;
            }
        }
        if (resolvedWidth != m_computedRect.width || resolvedHeight != m_computedRect.height) {
            if (!constraints.empty()) {
                m_computedRect = ResolveAnchoredRect({constraints.front()}, resolvedWidth, resolvedHeight, m_computedRect);
            } else {
                m_computedRect.width = resolvedWidth;
                m_computedRect.height = resolvedHeight;
            }
        }
        m_isDirty = false;

        const bool rectChanged = m_computedRect.x != previous.x || m_computedRect.y != previous.y ||
                                  m_computedRect.width != previous.width ||
                                  m_computedRect.height != previous.height;
        if (rectChanged) {
            LAMBUI_LOGT(TAG, "'{}' rect ({}, {}, {}, {}) -> ({}, {}, {}, {})", m_name,
                        previous.x, previous.y, previous.width, previous.height,
                        m_computedRect.x, m_computedRect.y, m_computedRect.width, m_computedRect.height);
            for (auto& child : m_children) child->MarkDirty();
        }
        if (rectChanged || m_paddingChanged) {
            LAMBUI_LOGT(TAG, "'{}' layout changed (padding={})", m_name, m_paddingChanged);
            m_paddingChanged = false;
            OnLayoutChanged();
        }
    }

    for (auto& child : m_children) child->ResolveLayout();
}

void UIWidget::SetFocusRingEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetFocusRingEnabled({})", GetName(), enabled);
    m_focusRingEnabled = enabled;
    MarkDirty();
}

void UIWidget::SetFocusRingColor(uint32_t color) {
    LAMBUI_LOGT(TAG, "'{}' SetFocusRingColor({})", GetName(), color);
    m_focusRingColor = color;
    MarkDirty();
}

void UIWidget::GenerateRenderCommandsWithFocus(std::vector<UIRenderCommand>& bucket) {
    if (!m_isVisible) return;
    GenerateRenderCommands(bucket);
    if (!m_hasManagerFocus || !m_focusRingEnabled || !m_isKeyboardEnabled) return;
    const auto& rect = m_computedRect;
    if (rect.width <= 0 || rect.height <= 0) return;
    for (int layer = 0; layer < 2; ++layer) {
        const float inset = static_cast<float>(layer);
        const float width = rect.width - 2 * inset;
        const float height = rect.height - 2 * inset;
        if (width <= 0 || height <= 0) break;
        const float thickness = std::min(1.0f, std::min(width, height) * 0.5f);
        const auto quad = [&](float x, float y, float quadWidth, float quadHeight) {
            if (quadWidth <= 0 || quadHeight <= 0) return;
            UIRenderCommand command;
            command.x = x;
            command.y = y;
            command.width = quadWidth;
            command.height = quadHeight;
            command.color = layer == 0 ? 0x172127FFu : m_focusRingColor;
            bucket.push_back(command);
        };
        quad(rect.x + inset, rect.y + inset, width, thickness);
        quad(rect.x + inset, rect.y + inset + height - thickness, width, thickness);
        quad(rect.x + inset, rect.y + inset + thickness, thickness, height - 2 * thickness);
        quad(rect.x + inset + width - thickness, rect.y + inset + thickness, thickness, height - 2 * thickness);
    }
}

void UIWidget::GenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!m_isVisible) return;
    AppendStyleShadow(bucket);
    GenerateContentRenderCommands(bucket);
}

void UIWidget::AppendStyleShadow(std::vector<UIRenderCommand>& bucket) const {
    if (!m_style || (m_style->shadowColor & 0xFFu) == 0 || m_computedRect.width <= 0 || m_computedRect.height <= 0) return;
    const auto& style = *m_style;
    const int layers = style.shadowBlur > 0 ? 8 : 1;
    const auto alpha = static_cast<uint32_t>(std::round(255.0f *
        (1.0f - std::pow(1.0f - (style.shadowColor & 0xFFu) / 255.0f, 1.0f / layers))));
    for (int layer = layers; layer > 0; --layer) {
        const float spread = style.shadowSpread + style.shadowBlur * (layer - 1) / layers;
        UIRenderCommand shadow;
        shadow.x = m_computedRect.x + style.shadowOffsetX - spread;
        shadow.y = m_computedRect.y + style.shadowOffsetY - spread;
        shadow.width = m_computedRect.width + spread * 2;
        shadow.height = m_computedRect.height + spread * 2;
        shadow.color = (style.shadowColor & 0xFFFFFF00u) | alpha;
        AppendStyledFill(bucket, shadow, style.cornerRadius + spread, UIFillPattern::Solid, 0, 1);
    }
}

void UIWidget::GenerateContentRenderCommands(std::vector<UIRenderCommand>& bucket) {
    GenerateOwnRenderCommands(bucket);
    GenerateChildRenderCommands(bucket);
}

void UIWidget::GenerateOwnRenderCommands(std::vector<UIRenderCommand>& bucket, const UIRect* contentClip) {
    if (!m_style && !contentClip) {
        OnGenerateRenderCommands(bucket);
    } else {
        std::vector<UIRenderCommand> ownCommands;
        OnGenerateRenderCommands(ownCommands);
        UIRenderCommand background;
        background.x = m_computedRect.x;
        background.y = m_computedRect.y;
        background.width = m_computedRect.width;
        background.height = m_computedRect.height;
        background.color = 0;
        const bool hasBackground = !ownCommands.empty() && ownCommands.front().type == RenderCommandType::DrawQuad &&
            ownCommands.front().x == background.x && ownCommands.front().y == background.y &&
            ownCommands.front().width == background.width && ownCommands.front().height == background.height &&
            (!contentClip || m_style || contentClip->x != background.x || contentClip->y != background.y ||
             contentClip->width != background.width || contentClip->height != background.height);
        if (hasBackground) background = ownCommands.front();
        if (m_style) {
            if (m_style->fillColor) background.color = *m_style->fillColor;
            AppendStyledFill(bucket, background, m_style->cornerRadius, m_style->pattern, m_style->patternColor, m_style->patternSize);
        } else if (hasBackground) {
            bucket.push_back(background);
        }
        if (contentClip) {
            UIRenderCommand clip;
            clip.type = RenderCommandType::PushScissor;
            clip.x = contentClip->x;
            clip.y = contentClip->y;
            clip.width = contentClip->width;
            clip.height = contentClip->height;
            bucket.push_back(clip);
        }
        for (size_t index = hasBackground ? 1 : 0; index < ownCommands.size(); ++index) {
            bucket.push_back(std::move(ownCommands[index]));
        }
    }
}

void UIWidget::GenerateChildRenderCommands(std::vector<UIRenderCommand>& bucket) {
    for (auto& child : m_children) child->GenerateRenderCommandsWithFocus(bucket);
}

void UIWidget::AppendChildRenderCommands(UIWidget& child, std::vector<UIRenderCommand>& bucket) {
    if (child.GetParent() == this) child.GenerateRenderCommandsWithFocus(bucket);
}

} // namespace LambUI
