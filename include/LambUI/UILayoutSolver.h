#pragma once

#include "lambui_export.h"
#include "UITypes.h"
#include <vector>

namespace LambUI {

// One fully-resolved anchor: "myPoint" on the widget being solved should
// align to "relativePoint" on relativeRect, offset by (xOffset, yOffset).
struct AnchorConstraint {
    AnchorPoint myPoint;
    UIRect relativeRect;
    AnchorPoint relativePoint;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
};

// Pure function: given a widget's anchor constraints plus its explicit size
// and previous rect, produce its new absolute screen rect. Has no knowledge
// of the widget tree, which keeps it trivially unit-testable.
LAMBUI_API UIRect ResolveAnchoredRect(const std::vector<AnchorConstraint>& constraints,
                                      float explicitWidth, float explicitHeight,
                                      const UIRect& previousRect);

} // namespace LambUI
