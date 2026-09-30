#include "LambUI/UIManager.h"
#include "LambUI/UIWidget.h"
#include <gtest/gtest.h>
#include <memory>

using namespace LambUI;

namespace {
class NullRenderer : public IRenderer {
public:
    void SubmitRenderCommands(const std::vector<UIRenderCommand>&) override {}
};
} // namespace

TEST(WidgetHierarchy, ChildAnchoredToRootResolvesAbsoluteRect) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIWidget* frame = manager.GetRoot().CreateChild<UIWidget>("Frame");
    frame->SetSize(200.0f, 60.0f);
    frame->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 20.0f, -20.0f);

    manager.Update(0.0f);

    const UIRect& rect = frame->GetComputedRect();
    EXPECT_FLOAT_EQ(rect.x, 20.0f);
    EXPECT_FLOAT_EQ(rect.y, -20.0f);
    EXPECT_FLOAT_EQ(rect.width, 200.0f);
    EXPECT_FLOAT_EQ(rect.height, 60.0f);
}

TEST(WidgetHierarchy, MovingParentCascadesDirtyFlagToChild) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    parent->SetSize(100.0f, 100.0f);
    parent->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    UIWidget* child = parent->CreateChild<UIWidget>("Child");
    child->SetSize(10.0f, 10.0f);
    child->SetPoint(AnchorPoint::TopLeft, parent, AnchorPoint::TopLeft, 5.0f, 5.0f);

    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(child->GetComputedRect().x, 5.0f);

    parent->ClearPoints();
    parent->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 50.0f, 50.0f);
    manager.Update(0.0f);

    EXPECT_FLOAT_EQ(child->GetComputedRect().x, 55.0f);
}
