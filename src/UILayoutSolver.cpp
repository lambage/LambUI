#include "LambUI/UILayoutSolver.h"
#include <optional>

namespace LambUI {

namespace {

enum class HAxis { Left, Right, CenterX, None };
enum class VAxis { Top, Bottom, CenterY, None };

HAxis HorizontalRole(AnchorPoint p) {
    switch (p) {
        case AnchorPoint::TopLeft:
        case AnchorPoint::BottomLeft:
        case AnchorPoint::Left:
            return HAxis::Left;
        case AnchorPoint::TopRight:
        case AnchorPoint::BottomRight:
        case AnchorPoint::Right:
            return HAxis::Right;
        case AnchorPoint::Center:
        case AnchorPoint::Top:
        case AnchorPoint::Bottom:
            return HAxis::CenterX;
    }
    return HAxis::None;
}

VAxis VerticalRole(AnchorPoint p) {
    switch (p) {
        case AnchorPoint::TopLeft:
        case AnchorPoint::TopRight:
        case AnchorPoint::Top:
            return VAxis::Top;
        case AnchorPoint::BottomLeft:
        case AnchorPoint::BottomRight:
        case AnchorPoint::Bottom:
            return VAxis::Bottom;
        case AnchorPoint::Center:
        case AnchorPoint::Left:
        case AnchorPoint::Right:
            return VAxis::CenterY;
    }
    return VAxis::None;
}

float HorizontalPointOnRect(const UIRect& rect, AnchorPoint p) {
    switch (HorizontalRole(p)) {
        case HAxis::Left: return rect.x;
        case HAxis::Right: return rect.x + rect.width;
        case HAxis::CenterX: return rect.x + rect.width * 0.5f;
        default: return rect.x;
    }
}

float VerticalPointOnRect(const UIRect& rect, AnchorPoint p) {
    switch (VerticalRole(p)) {
        case VAxis::Top: return rect.y;
        case VAxis::Bottom: return rect.y + rect.height;
        case VAxis::CenterY: return rect.y + rect.height * 0.5f;
        default: return rect.y;
    }
}

} // namespace

UIRect ResolveAnchoredRect(const std::vector<AnchorConstraint>& constraints,
                           float explicitWidth, float explicitHeight,
                           const UIRect& previousRect) {
    std::optional<float> left, right, centerX, top, bottom, centerY;

    for (const auto& c : constraints) {
        const float targetX = HorizontalPointOnRect(c.relativeRect, c.relativePoint) + c.xOffset;
        const float targetY = VerticalPointOnRect(c.relativeRect, c.relativePoint) + c.yOffset;

        switch (HorizontalRole(c.myPoint)) {
            case HAxis::Left: left = targetX; break;
            case HAxis::Right: right = targetX; break;
            case HAxis::CenterX: centerX = targetX; break;
            default: break;
        }
        switch (VerticalRole(c.myPoint)) {
            case VAxis::Top: top = targetY; break;
            case VAxis::Bottom: bottom = targetY; break;
            case VAxis::CenterY: centerY = targetY; break;
            default: break;
        }
    }

    UIRect result = previousRect;

    if (left && right) {
        result.x = *left;
        result.width = *right - *left;
    } else if (left) {
        result.x = *left;
        result.width = explicitWidth;
    } else if (right) {
        result.width = explicitWidth;
        result.x = *right - explicitWidth;
    } else if (centerX) {
        result.width = explicitWidth;
        result.x = *centerX - explicitWidth * 0.5f;
    }

    if (top && bottom) {
        result.y = *top;
        result.height = *bottom - *top;
    } else if (top) {
        result.y = *top;
        result.height = explicitHeight;
    } else if (bottom) {
        result.height = explicitHeight;
        result.y = *bottom - explicitHeight;
    } else if (centerY) {
        result.height = explicitHeight;
        result.y = *centerY - explicitHeight * 0.5f;
    }

    return result;
}

} // namespace LambUI
