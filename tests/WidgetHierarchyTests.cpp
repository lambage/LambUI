#include "LambUI/UIManager.h"
#include "LambUI/UIWidget.h"
#include "LambUI/UIButton.h"
#include "LambUI/UITextureWidget.h"
#include "LambUI/UIScrollContainer.h"
#include "LambUI/UITreeView.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

using namespace LambUI;

namespace {
class NullRenderer : public IRenderer {
public:
    std::vector<UIRenderCommand> commands;
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& bucket) override { commands = bucket; }
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

TEST(WidgetLayout, PaddingMarginAndRelativeSizeFollowParentChanges) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(400, 300);
    auto& root = manager.GetRoot();
    root.SetPadding({10, 20, 30, 40});
    auto* child = root.CreateChild<UIWidget>("relative");
    child->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft);
    child->SetMargin({5, 6, 15, 14});
    child->SetRelativeSize(0.5f, 1.0f);
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().x, 15);
    EXPECT_FLOAT_EQ(child->GetComputedRect().y, 26);
    EXPECT_FLOAT_EQ(child->GetComputedRect().width, 170);
    EXPECT_FLOAT_EQ(child->GetComputedRect().height, 220);
    root.SetPadding({20, 20, 20, 20});
    manager.SetDisplaySize(600, 400);
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().x, 25);
    EXPECT_FLOAT_EQ(child->GetComputedRect().width, 270);
    EXPECT_FLOAT_EQ(child->GetComputedRect().height, 340);
    child->SetSize(80, 60);
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().width, 80);
}

TEST(WidgetLayout, StretchInsetsAndConstraintsPreserveFirstAnchor) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(400, 300);
    auto& root = manager.GetRoot();
    root.SetPadding({10, 20, 30, 40});
    auto* child = root.CreateChild<UIWidget>("stretch");
    child->SetAllPoints(&root);
    child->SetMargin({5, 6, 15, 14});
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().width, 340);
    EXPECT_FLOAT_EQ(child->GetComputedRect().height, 220);
    child->SetMaxSize(200, 200);
    child->SetAspectRatio(2);
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().x, 15);
    EXPECT_FLOAT_EQ(child->GetComputedRect().y, 26);
    EXPECT_FLOAT_EQ(child->GetComputedRect().width, 200);
    EXPECT_FLOAT_EQ(child->GetComputedRect().height, 100);
    child->ClearPoints();
    child->SetPoint(AnchorPoint::BottomRight, &root, AnchorPoint::BottomRight);
    child->SetSize(300, 300);
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().x, 155);
    EXPECT_FLOAT_EQ(child->GetComputedRect().y, 146);
}

TEST(WidgetLayout, LimitsWinImpossibleAspectAndMinimumWinsMaximum) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* child = manager.GetRoot().CreateChild<UIWidget>("limits");
    child->SetSize(20, 20);
    child->SetMinSize(100, 60);
    child->SetMaxSize(80, 70);
    child->SetAspectRatio(4);
    manager.Update(0);
    EXPECT_FLOAT_EQ(child->GetComputedRect().width, 100);
    EXPECT_FLOAT_EQ(child->GetComputedRect().height, 60);
}

TEST(WidgetStyle, RoundedPatternsPreserveTextureUvsAndClearRestoresOriginal) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* widget = manager.GetRoot().CreateChild<UITextureWidget>("pattern");
    widget->SetSize(40, 30);
    widget->SetUVRect(0.2f, 0.3f, 0.8f, 0.9f);
    widget->SetTexture(widget);
    UIStyle style;
    style.fillColor = 0x123456FFu;
    style.cornerRadius = 8;
    style.pattern = UIFillPattern::Checkerboard;
    style.patternColor = 0xABCDEF80u;
    widget->SetStyle(style);
    manager.Update(0);
    manager.Render();
    ASSERT_GT(renderer->commands.size(), 1u);
    bool alternate = false;
    for (const auto& command : renderer->commands) {
        EXPECT_EQ(command.type, RenderCommandType::DrawQuad);
        EXPECT_EQ(command.textureHandle, widget);
        EXPECT_GE(command.x, 0);
        EXPECT_GE(command.y, 0);
        EXPECT_LE(command.x + command.width, 40);
        EXPECT_LE(command.y + command.height, 30);
        EXPECT_GT(command.width, 0);
        EXPECT_GT(command.height, 0);
        EXPECT_NEAR(command.u0, 0.2f + 0.6f * command.x / 40, 0.00001f);
        EXPECT_NEAR(command.v1, 0.3f + 0.6f * (command.y + command.height) / 30, 0.00001f);
        EXPECT_FALSE(command.x <= 0 && command.y <= 0);
        alternate |= command.color == style.patternColor;
    }
    EXPECT_TRUE(alternate);
    widget->ClearStyle();
    manager.Render();
    ASSERT_EQ(renderer->commands.size(), 1u);
    EXPECT_FLOAT_EQ(renderer->commands.front().width, 40);
    EXPECT_FLOAT_EQ(renderer->commands.front().u0, 0.2f);
}

TEST(WidgetStyle, ButtonStateColorsSurviveAndExplicitFillOverrides) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    manager.SetDisplaySize(100, 100);
    auto* button = manager.GetRoot().CreateChild<UIButton>("button");
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 10, 10);
    button->SetSize(40, 30);
    button->SetNormalColor(0x123456FFu);
    button->SetHoverColor(0xABCDEF80u);
    UIStyle style;
    style.cornerRadius = 6;
    button->SetStyle(style);
    manager.Update(0);
    manager.Render();
    ASSERT_FALSE(renderer->commands.empty());
    EXPECT_EQ(renderer->commands.front().color, 0x123456FFu);
    manager.InjectMouseMove(20, 20);
    manager.Render();
    EXPECT_EQ(renderer->commands.front().color, 0xABCDEF80u);
    style.fillColor = 0xAABBCCFFu;
    button->SetStyle(style);
    manager.Render();
    EXPECT_EQ(renderer->commands.front().color, 0xAABBCCFFu);
}

TEST(WidgetStyle, ContainerShadowPrecedesOwnClipAndChildrenFollowBackground) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* container = manager.GetRoot().CreateChild<UIScrollContainer>("styled");
    container->SetSize(80, 60);
    container->SetScrollbarsEnabled(false);
    UIStyle style;
    style.fillColor = 0x123456FFu;
    style.shadowColor = 0x00000080u;
    style.shadowBlur = 8;
    style.shadowOffsetY = 4;
    container->SetStyle(style);
    auto* child = container->GetContent()->CreateChild<UITextureWidget>("child");
    child->SetSize(10, 10);
    child->SetTint(0xABCDEF80u);
    manager.Update(0);
    manager.Render();
    ASSERT_GT(renderer->commands.size(), 4u);
    EXPECT_EQ(renderer->commands.front().type, RenderCommandType::DrawQuad);
    EXPECT_LT(renderer->commands.front().x, 0);
    auto clip = std::find_if(renderer->commands.begin(), renderer->commands.end(), [](const auto& command) {
        return command.type == RenderCommandType::PushScissor;
    });
    ASSERT_NE(clip, renderer->commands.end());
    ASSERT_NE(clip, renderer->commands.begin());
    EXPECT_EQ((clip - 1)->color, 0x123456FFu);
    EXPECT_EQ(renderer->commands[renderer->commands.size() - 2].color, 0xABCDEF80u);
    EXPECT_EQ(renderer->commands.back().type, RenderCommandType::PopScissor);
    container->SetVisible(false);
    manager.Render();
    EXPECT_TRUE(renderer->commands.empty());
}

TEST(WidgetLayout, ScrollPaddingUpdatesRangeClippingAndInjectedHits) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    manager.SetDisplaySize(200, 200);
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("padded");
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetSize(100, 100);
    scroll->SetContentSize(200, 200);
    scroll->SetScrollbarsEnabled(false);
    auto* child = scroll->GetContent()->CreateChild<UIButton>("target");
    child->SetAllPoints(scroll->GetContent());
    int clicks = 0;
    child->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0);
    scroll->SetPadding({10, 20, 30, 40});
    manager.Update(0);
    scroll->SetScrollOffset(1000, 1000);
    manager.Update(0);
    EXPECT_FLOAT_EQ(scroll->GetScrollX(), 140);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 160);
    manager.Render();
    ASSERT_FALSE(renderer->commands.empty());
    EXPECT_EQ(renderer->commands.front().type, RenderCommandType::PushScissor);
    EXPECT_FLOAT_EQ(renderer->commands.front().x, 10);
    EXPECT_FLOAT_EQ(renderer->commands.front().y, 20);
    EXPECT_FLOAT_EQ(renderer->commands.front().width, 60);
    EXPECT_FLOAT_EQ(renderer->commands.front().height, 40);
    manager.InjectMouseMove(5, 25);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectMouseMove(15, 25);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(clicks, 1);
    scroll->SetScrollOffset(1000, 1000);
    scroll->SetPadding({});
    manager.Update(0);
    EXPECT_FLOAT_EQ(scroll->GetScrollX(), 100);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 100);
}

TEST(WidgetStyle, TreeRowsRemainInsidePaddedClip) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("tree");
    tree->SetSize(80, 40);
    tree->SetPadding({4, 4, 4, 4});
    tree->AddNode(UITreeView::RootNode, "row");
    manager.Update(0);
    manager.Render();
    bool clipped = false;
    bool sawText = false;
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::PushScissor) clipped = true;
        if (command.type == RenderCommandType::PopScissor) clipped = false;
        if (command.type == RenderCommandType::DrawString) {
            EXPECT_TRUE(clipped);
            sawText = true;
        }
    }
    EXPECT_TRUE(sawText);
}
