#include "LambUI/LambUI.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

using namespace LambUI;

namespace {
class RecordingRenderer : public IRenderer {
public:
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) override { bucket = commands; }
    std::vector<UIRenderCommand> bucket;
};

class CompoundWidgets : public testing::Test {
protected:
    std::shared_ptr<RecordingRenderer> renderer = std::make_shared<RecordingRenderer>();
    UIManager manager{renderer};

    void Click(float mouseX, float mouseY, MouseButton button = MouseButton::Left) {
        manager.Update(0.0f);
        manager.InjectMouseMove(mouseX, mouseY);
        manager.InjectMouseButton(button, true);
        manager.InjectMouseButton(button, false);
    }
};
}

TEST_F(CompoundWidgets, ProgressBarClampsAndEmitsHorizontalAndVerticalFill) {
    auto* bar = manager.GetRoot().CreateChild<UIProgressBar>("Progress");
    bar->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 10.0f, 20.0f);
    bar->SetSize(200.0f, 40.0f);
    bar->SetMinMaxValues(0.0f, 100.0f);
    int changes = 0;
    bar->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    bar->SetValue(25.0f);
    manager.Update(0.0f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 2u);
    EXPECT_FLOAT_EQ(renderer->bucket.back().width, 50.0f);
    EXPECT_FALSE(bar->IsMouseEnabled());
    bar->SetOrientation(ProgressOrientation::Vertical);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket.back().height, 10.0f);
    EXPECT_FLOAT_EQ(renderer->bucket.back().y, 50.0f);
    bar->SetValue(500.0f);
    bar->SetValue(500.0f);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket.back().height, 40.0f);
    EXPECT_EQ(changes, 2);
    bar->SetValue(-1.0f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 1u);
}

TEST_F(CompoundWidgets, ProgressBarHandlesReversedDegenerateAndNonfiniteRanges) {
    auto* bar = manager.GetRoot().CreateChild<UIProgressBar>();
    bar->SetMinMaxValues(20.0f, 10.0f);
    EXPECT_FLOAT_EQ(bar->GetMinValue(), 10.0f);
    EXPECT_FLOAT_EQ(bar->GetMaxValue(), 20.0f);
    EXPECT_FLOAT_EQ(bar->GetValue(), 10.0f);
    bar->SetValue(std::numeric_limits<float>::quiet_NaN());
    bar->SetMinMaxValues(0.0f, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(bar->GetValue(), 10.0f);
    EXPECT_FLOAT_EQ(bar->GetMaxValue(), 20.0f);
    bar->SetMinMaxValues(5.0f, 5.0f);
    manager.Update(0.0f);
    manager.Render();
    EXPECT_FLOAT_EQ(bar->GetValue(), 5.0f);
    EXPECT_EQ(renderer->bucket.size(), 1u);
}

TEST_F(CompoundWidgets, TabsSwitchThroughInjectionAndPreserveHiddenPageState) {
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>("Tabs");
    tabs->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tabs->SetSize(200.0f, 150.0f);
    auto* first = tabs->AddTab("First");
    auto* second = tabs->AddTab("Second");
    auto* firstButton = first->CreateChild<UIButton>();
    firstButton->SetAllPoints(first);
    int clicks = 0;
    int changes = 0;
    firstButton->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    tabs->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    Click(20.0f, 50.0f);
    EXPECT_EQ(clicks, 1);
    Click(150.0f, 10.0f, MouseButton::Right);
    EXPECT_EQ(tabs->GetSelectedIndex(), 0);
    Click(150.0f, 10.0f);
    EXPECT_EQ(tabs->GetSelectedIndex(), 1);
    EXPECT_FALSE(first->IsVisible());
    EXPECT_TRUE(second->IsVisible());
    Click(20.0f, 50.0f);
    EXPECT_EQ(clicks, 1);
    tabs->SetSelectedIndex(1);
    tabs->SetSelectedIndex(99);
    EXPECT_EQ(changes, 1);
    tabs->SetSize(300.0f, 200.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(second->GetComputedRect().width, 300.0f);
    EXPECT_FLOAT_EQ(second->GetComputedRect().height, 172.0f);
    Click(20.0f, 10.0f);
    Click(20.0f, 50.0f);
    EXPECT_EQ(clicks, 2);
    EXPECT_EQ(tabs->GetPage(0), first);
    EXPECT_EQ(tabs->GetPage(-1), nullptr);
}

TEST_F(CompoundWidgets, TreeExpansionSelectionScrollingAndClear) {
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("Tree");
    tree->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tree->SetSize(200.0f, 48.0f);
    const auto parent = tree->AddNode(UITreeView::RootNode, "Parent");
    const auto child = tree->AddNode(parent, "Child");
    const auto grandchild = tree->AddNode(child, "Grandchild");
    tree->AddNode(UITreeView::RootNode, "Sibling");
    EXPECT_EQ(tree->AddNode(99, "Invalid"), UITreeView::RootNode);
    int changes = 0;
    tree->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    Click(60.0f, 36.0f);
    EXPECT_EQ(tree->GetSelectedNode(), child);
    Click(5.0f, 12.0f);
    EXPECT_FALSE(tree->IsExpanded(parent));
    EXPECT_EQ(tree->GetVisibleNodeCount(), 2u);
    EXPECT_EQ(tree->GetSelectedNode(), child);
    EXPECT_EQ(changes, 1);
    Click(5.0f, 12.0f);
    EXPECT_EQ(tree->GetVisibleNodeCount(), 4u);
    manager.InjectMouseWheel(0.0f, -48.0f);
    EXPECT_FLOAT_EQ(tree->GetScrollY(), 48.0f);
    Click(70.0f, 12.0f);
    EXPECT_EQ(tree->GetSelectedNode(), grandchild);
    manager.Render();
    EXPECT_EQ(renderer->bucket.front().type, RenderCommandType::PushScissor);
    EXPECT_EQ(renderer->bucket.back().type, RenderCommandType::PopScissor);
    tree->SetExpanded(parent, false);
    EXPECT_FLOAT_EQ(tree->GetScrollY(), 0.0f);
    tree->ClearNodes();
    EXPECT_EQ(tree->GetVisibleNodeCount(), 0u);
    EXPECT_EQ(tree->GetSelectedNode(), UITreeView::RootNode);
    EXPECT_EQ(changes, 3);
}

TEST_F(CompoundWidgets, PopupRendersLastClampsAndConsumesOutsideDismissal) {
    manager.SetDisplaySize(200.0f, 100.0f);
    auto* popup = manager.GetOverlayRoot().CreateChild<UIButton>("Popup");
    popup->SetSize(80.0f, 50.0f);
    popup->SetNormalColor(0xFF112233u);
    auto* background = manager.GetRoot().CreateChild<UIButton>("Background");
    background->SetAllPoints(&manager.GetRoot());
    int backgroundClicks = 0;
    background->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++backgroundClicks; });
    manager.ShowPopup(*popup, 190.0f, 95.0f);
    manager.Update(0.0f);
    manager.Render();
    EXPECT_FLOAT_EQ(popup->GetComputedRect().x, 120.0f);
    EXPECT_FLOAT_EQ(popup->GetComputedRect().y, 50.0f);
    EXPECT_EQ(renderer->bucket.back().color, 0xFF112233u);
    Click(10.0f, 10.0f);
    EXPECT_EQ(manager.GetActivePopup(), nullptr);
    EXPECT_EQ(backgroundClicks, 0);
    Click(10.0f, 10.0f);
    EXPECT_EQ(backgroundClicks, 1);
    manager.ShowPopup(*popup, 0.0f, 0.0f);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    EXPECT_EQ(manager.GetActivePopup(), nullptr);
}

TEST_F(CompoundWidgets, TooltipUsesInjectedTimeClampsAndDoesNotCaptureInput) {
    manager.SetDisplaySize(120.0f, 60.0f);
    manager.SetTooltipDelay(0.5f);
    auto* button = manager.GetRoot().CreateChild<UIButton>();
    button->SetAllPoints(&manager.GetRoot());
    button->SetTooltip("Details");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0.0f);
    manager.InjectMouseMove(115.0f, 55.0f);
    manager.Update(0.49f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 1u);
    manager.Update(0.01f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[3].text, "Details");
    EXPECT_LE(renderer->bucket[1].x + renderer->bucket[1].width, 120.0f);
    EXPECT_LE(renderer->bucket[1].y + renderer->bucket[1].height, 60.0f);
    Click(115.0f, 55.0f);
    EXPECT_EQ(clicks, 1);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 1u);
    manager.Update(0.5f);
    button->SetVisible(false);
    manager.Update(0.0f);
    manager.Render();
    EXPECT_TRUE(renderer->bucket.empty());
}

TEST_F(CompoundWidgets, ContextMenuActionsDisabledRowsReplacementAndScrolling) {
    manager.SetDisplaySize(200.0f, 72.0f);
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Context");
    int actions = 0;
    menu->SetItems({{"Disabled", [&] { actions += 100; }, false},
                    {"", {}, true, true}, {"Run", [&] { ++actions; }},
                    {"Last", [&] { actions += 10; }}});
    menu->Open(0.0f, 0.0f);
    Click(20.0f, 12.0f);
    EXPECT_TRUE(menu->IsOpen());
    EXPECT_EQ(actions, 0);
    Click(20.0f, 36.0f);
    EXPECT_TRUE(menu->IsOpen());
    manager.InjectMouseWheel(0.0f, -24.0f);
    EXPECT_FLOAT_EQ(menu->GetScrollY(), 24.0f);
    Click(20.0f, 60.0f);
    EXPECT_EQ(actions, 10);
    EXPECT_FALSE(menu->IsOpen());
    menu->SetItems({{"Replacement", [&] { ++actions; }}});
    menu->Open(0.0f, 0.0f);
    Click(20.0f, 12.0f, MouseButton::Right);
    EXPECT_TRUE(menu->IsOpen());
    Click(20.0f, 12.0f);
    EXPECT_EQ(actions, 11);
    menu->SetItems({});
    menu->Open(0.0f, 0.0f);
    EXPECT_FALSE(menu->IsOpen());
}

TEST_F(CompoundWidgets, MenuBarOpensSwitchesOnHoverTogglesAndClosesWithHiddenOwner) {
    auto* bar = manager.GetRoot().CreateChild<UIMenuBar>(manager, "MenuBar");
    bar->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    bar->SetSize(200.0f, 28.0f);
    int actions = 0;
    auto* first = bar->AddMenu("File", {{"Open", [&] { ++actions; }}});
    auto* second = bar->AddMenu("Edit", {{"Copy", [&] { actions += 10; }}});
    Click(20.0f, 10.0f);
    EXPECT_TRUE(first->IsOpen());
    manager.InjectMouseMove(150.0f, 10.0f);
    EXPECT_FALSE(first->IsOpen());
    EXPECT_TRUE(second->IsOpen());
    Click(120.0f, 40.0f);
    EXPECT_EQ(actions, 10);
    Click(20.0f, 10.0f);
    EXPECT_TRUE(first->IsOpen());
    Click(20.0f, 10.0f);
    EXPECT_FALSE(first->IsOpen());
    Click(20.0f, 10.0f);
    bar->SetVisible(false);
    manager.Update(0.0f);
    EXPECT_FALSE(first->IsOpen());
    EXPECT_EQ(bar->GetMenuCount(), 2u);
    EXPECT_EQ(bar->GetMenu(2), nullptr);
}

TEST_F(CompoundWidgets, PopupResizesBackAfterViewportGrowthAndEscapesAncestorClipping) {
    manager.SetDisplaySize(400.0f, 200.0f);
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>();
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetSize(40.0f, 40.0f);
    scroll->SetContentSize(40.0f, 40.0f);
    auto* owner = scroll->GetContent()->CreateChild<UIButton>();
    owner->SetAllPoints(scroll->GetContent());
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager);
    int actions = 0;
    menu->SetItems({{"Action", [&] { ++actions; }}});
    owner->RegisterCallback(UIEventType::OnClick, [&](const UIEventData& data) {
        if (data.button == MouseButton::Right) menu->Open(50.0f, 50.0f, owner);
    });
    Click(10.0f, 10.0f, MouseButton::Right);
    EXPECT_TRUE(menu->IsOpen());
    manager.Render();
    EXPECT_EQ(renderer->bucket[2].type, RenderCommandType::PopScissor);
    EXPECT_EQ(renderer->bucket[3].type, RenderCommandType::PushScissor);
    manager.SetDisplaySize(80.0f, 60.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(menu->GetComputedRect().width, 80.0f);
    manager.SetDisplaySize(400.0f, 200.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(menu->GetComputedRect().width, 160.0f);
    Click(60.0f, 60.0f);
    EXPECT_EQ(actions, 1);
}

TEST_F(CompoundWidgets, MenuActionMayReplaceItemsAndReopenSafely) {
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager);
    int actions = 0;
    menu->SetItems({{"Replace", [&] {
        ++actions;
        menu->SetItems({{"New action", [&] { actions += 10; }}});
        menu->Open(0.0f, 0.0f);
    }}});
    menu->Open(0.0f, 0.0f);
    Click(10.0f, 10.0f);
    EXPECT_TRUE(menu->IsOpen());
    EXPECT_EQ(actions, 1);
    Click(10.0f, 10.0f);
    EXPECT_EQ(actions, 11);
    EXPECT_FALSE(menu->IsOpen());
}

TEST_F(CompoundWidgets, HiddenTabInputDoesNotReceiveCharactersAndPopupInputDoes) {
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>();
    tabs->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tabs->SetSize(200.0f, 100.0f);
    auto* first = tabs->AddTab("First");
    tabs->AddTab("Second");
    auto* input = first->CreateChild<UIInputBox>();
    input->SetAllPoints(first);
    Click(10.0f, 40.0f);
    manager.InjectCharacter(U'a');
    tabs->SetSelectedIndex(1);
    manager.InjectCharacter(U'b');
    EXPECT_EQ(input->GetText(), "a");
    auto* popup = manager.GetOverlayRoot().CreateChild<UIWidget>();
    popup->SetSize(100.0f, 40.0f);
    auto* popupInput = popup->CreateChild<UIInputBox>();
    popupInput->SetAllPoints(popup);
    manager.ShowPopup(*popup, 0.0f, 0.0f);
    Click(10.0f, 10.0f);
    manager.InjectCharacter(U'x');
    EXPECT_EQ(popupInput->GetText(), "x");
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(popupInput->GetText(), "");
}