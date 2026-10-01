#include "LambUI/UIManager.h"
#include "LambUI/UIScrollContainer.h"
#include "LambUI/UITextureWidget.h"
#include "LambUI/UIWidget.h"
#include <gtest/gtest.h>
#include <memory>

using namespace LambUI;

namespace {
class NullRenderer : public IRenderer {
public:
    std::vector<UIRenderCommand> commands;
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& bucket) override {
        commands = bucket;
    }
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
    manager.InjectMouseWheel(0.0f, -20.0f);

    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 20.0f);

    manager.InjectMouseWheel(0.0f, 10.0f);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 10.0f);
}

TEST(ScrollContainer, ClippedAndHiddenContentDoesNotReceiveInput) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIScrollContainer* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 100.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetContentSize(100.0f, 300.0f);
    UIWidget* item = scroll->GetContent()->CreateChild<UIWidget>("Item");
    item->SetAllPoints(scroll->GetContent());
    int clicks = 0;
    item->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 150.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectMouseWheel(0.0f, -20.0f);
    EXPECT_EQ(clicks, 0);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 0.0f);

    scroll->SetVisible(false);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectMouseWheel(0.0f, -20.0f);
    EXPECT_EQ(clicks, 0);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 0.0f);

    scroll->SetVisible(true);
    scroll->SetMouseEnabled(false);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(clicks, 1);
}

TEST(ScrollContainer, LayoutChangesReclampOffsetsBeforeLayingOutContent) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIScrollContainer* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 100.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetContentSize(300.0f, 300.0f);
    scroll->SetScrollOffset(1000.0f, 1000.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(scroll->GetScrollX(), 200.0f);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 200.0f);

    scroll->SetSize(250.0f, 400.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(scroll->GetScrollX(), 50.0f);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 0.0f);
    EXPECT_FLOAT_EQ(scroll->GetContent()->GetComputedRect().x, -50.0f);
    EXPECT_FLOAT_EQ(scroll->GetContent()->GetComputedRect().y, 0.0f);
}

TEST(ScrollContainer, RenderingWrapsContentInScissorCommands) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    UIScrollContainer* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 100.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 20.0f, 30.0f);
    scroll->SetContentSize(100.0f, 300.0f);
    UITextureWidget* item = scroll->GetContent()->CreateChild<UITextureWidget>("Item");
    item->SetAllPoints(scroll->GetContent());
    manager.Update(0.0f);
    manager.Render();

    ASSERT_EQ(renderer->commands.size(), 3u);
    EXPECT_EQ(renderer->commands[0].type, RenderCommandType::PushScissor);
    EXPECT_FLOAT_EQ(renderer->commands[0].x, 20.0f);
    EXPECT_FLOAT_EQ(renderer->commands[0].y, 30.0f);
    EXPECT_FLOAT_EQ(renderer->commands[0].width, 100.0f);
    EXPECT_FLOAT_EQ(renderer->commands[0].height, 100.0f);
    EXPECT_EQ(renderer->commands[1].type, RenderCommandType::DrawQuad);
    EXPECT_EQ(renderer->commands[2].type, RenderCommandType::PopScissor);

    scroll->SetVisible(false);
    manager.Render();
    EXPECT_TRUE(renderer->commands.empty());
}

TEST(ScrollContainer, NestedContainersRespectAncestorClipAndRouteWheelToNearest) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    UIScrollContainer* outer = manager.GetRoot().CreateChild<UIScrollContainer>("Outer");
    outer->SetSize(100.0f, 100.0f);
    outer->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    outer->SetContentSize(300.0f, 300.0f);
    UIScrollContainer* inner = outer->GetContent()->CreateChild<UIScrollContainer>("Inner");
    inner->SetSize(100.0f, 100.0f);
    inner->SetPoint(AnchorPoint::TopLeft, outer->GetContent(), AnchorPoint::TopLeft, 50.0f, 50.0f);
    inner->SetContentSize(200.0f, 200.0f);
    UITextureWidget* item = inner->GetContent()->CreateChild<UITextureWidget>("Item");
    item->SetAllPoints(inner->GetContent());
    int clicks = 0;
    item->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    UITextureWidget* sibling = outer->GetContent()->CreateChild<UITextureWidget>("Sibling");
    sibling->SetSize(10.0f, 10.0f);
    sibling->SetPoint(AnchorPoint::TopLeft, outer->GetContent(), AnchorPoint::TopLeft);

    manager.Update(0.0f);
    manager.InjectMouseMove(120.0f, 60.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectMouseWheel(-20.0f, -20.0f);
    EXPECT_EQ(clicks, 0);
    EXPECT_FLOAT_EQ(inner->GetScrollY(), 0.0f);

    manager.InjectMouseMove(60.0f, 60.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectMouseWheel(-20.0f, -20.0f);
    EXPECT_EQ(clicks, 1);
    EXPECT_FLOAT_EQ(inner->GetScrollX(), 20.0f);
    EXPECT_FLOAT_EQ(inner->GetScrollY(), 20.0f);
    EXPECT_FLOAT_EQ(outer->GetScrollX(), 0.0f);
    EXPECT_FLOAT_EQ(outer->GetScrollY(), 0.0f);

    manager.Update(0.0f);
    manager.Render();
    ASSERT_EQ(renderer->commands.size(), 6u);
    EXPECT_EQ(renderer->commands[0].type, RenderCommandType::PushScissor);
    EXPECT_EQ(renderer->commands[1].type, RenderCommandType::PushScissor);
    EXPECT_EQ(renderer->commands[2].type, RenderCommandType::DrawQuad);
    EXPECT_EQ(renderer->commands[3].type, RenderCommandType::PopScissor);
    EXPECT_EQ(renderer->commands[4].type, RenderCommandType::DrawQuad);
    EXPECT_EQ(renderer->commands[5].type, RenderCommandType::PopScissor);
}
