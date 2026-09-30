#include "LambUI/UILayoutSolver.h"
#include <gtest/gtest.h>

using namespace LambUI;

TEST(LayoutSolver, TopLeftWithExplicitSizeOffsetsFromParent) {
    UIRect parentRect{0.0f, 0.0f, 200.0f, 100.0f};
    std::vector<AnchorConstraint> constraints = {
        {AnchorPoint::TopLeft, parentRect, AnchorPoint::TopLeft, 20.0f, -20.0f}
    };

    UIRect result = ResolveAnchoredRect(constraints, 50.0f, 30.0f, UIRect{});

    EXPECT_FLOAT_EQ(result.x, 20.0f);
    EXPECT_FLOAT_EQ(result.y, -20.0f);
    EXPECT_FLOAT_EQ(result.width, 50.0f);
    EXPECT_FLOAT_EQ(result.height, 30.0f);
}

TEST(LayoutSolver, TopLeftAndBottomRightStretchesToFillParent) {
    UIRect parentRect{10.0f, 10.0f, 200.0f, 100.0f};
    std::vector<AnchorConstraint> constraints = {
        {AnchorPoint::TopLeft, parentRect, AnchorPoint::TopLeft, 0.0f, 0.0f},
        {AnchorPoint::BottomRight, parentRect, AnchorPoint::BottomRight, 0.0f, 0.0f}
    };

    UIRect result = ResolveAnchoredRect(constraints, 0.0f, 0.0f, UIRect{});

    EXPECT_FLOAT_EQ(result.x, 10.0f);
    EXPECT_FLOAT_EQ(result.y, 10.0f);
    EXPECT_FLOAT_EQ(result.width, 200.0f);
    EXPECT_FLOAT_EQ(result.height, 100.0f);
}

TEST(LayoutSolver, CenterAnchorCentersExplicitSize) {
    UIRect parentRect{0.0f, 0.0f, 100.0f, 100.0f};
    std::vector<AnchorConstraint> constraints = {
        {AnchorPoint::Center, parentRect, AnchorPoint::Center, 0.0f, 0.0f}
    };

    UIRect result = ResolveAnchoredRect(constraints, 20.0f, 10.0f, UIRect{});

    EXPECT_FLOAT_EQ(result.x, 40.0f);
    EXPECT_FLOAT_EQ(result.y, 45.0f);
    EXPECT_FLOAT_EQ(result.width, 20.0f);
    EXPECT_FLOAT_EQ(result.height, 10.0f);
}
