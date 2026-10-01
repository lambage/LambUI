#include "LambUI/UIManager.h"
#include "LambUI/UIScrollContainer.h"
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

TEST(ScrollContainer, ScrollOffsetIsClampedToContentOverflow) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIScrollContainer* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 100.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);
    scroll->SetContentSize(100.0f, 300.0f);

    manager.Update(0.0f);

    scroll->SetScrollOffset(0.0f, 1000.0f); // way past the bottom
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 200.0f); // 300 (content) - 100 (viewport)

    scroll->SetScrollOffset(0.0f, -50.0f); // above the top
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 0.0f);
}

TEST(ScrollContainer, ScrollOffsetTranslatesContentRect) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIScrollContainer* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 100.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);
    scroll->SetContentSize(100.0f, 300.0f);

    UIWidget* item = scroll->GetContent()->CreateChild<UIWidget>("Item");
    item->SetSize(20.0f, 20.0f);
    item->SetPoint(AnchorPoint::TopLeft, scroll->GetContent(), AnchorPoint::TopLeft, 0.0f, 40.0f);

    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(item->GetComputedRect().y, 40.0f);

    scroll->SetScrollOffset(0.0f, 30.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(item->GetComputedRect().y, 10.0f);
}

TEST(ScrollContainer, InjectMouseWheelRoutesToAncestorScrollContainer) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIScrollContainer* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 100.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);
    scroll->SetContentSize(100.0f, 300.0f);

    UIWidget* item = scroll->GetContent()->CreateChild<UIWidget>("Item");
    item->SetSize(20.0f, 20.0f);
    item->SetPoint(AnchorPoint::TopLeft, scroll->GetContent(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f); // over the item, inside the scroll viewport
    manager.InjectMouseWheel(0.0f, 20.0f);

    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 20.0f);
}
