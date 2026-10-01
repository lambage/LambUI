#include "LambUI/UIButton.h"
#include "LambUI/UICheckBox.h"
#include "LambUI/UIDropDownBox.h"
#include "LambUI/UIInputBox.h"
#include "LambUI/UIManager.h"
#include "LambUI/UISlider.h"
#include "LambUI/UITabControl.h"
#include "LambUI/UITreeView.h"
#include "LambUI/UIMenuBar.h"
#include "LambUI/UIRadioButton.h"
#include "LambUI/UITextureWidget.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace LambUI;

namespace {
class NullRenderer : public IRenderer {
public:
    std::vector<UIRenderCommand> commands;
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& bucket) override { commands = bucket; }
};
} // namespace

TEST(FocusVisualization, RingFollowsFocusAndHonorsAppearanceAndEligibility) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    for (auto* widget : std::vector<UIWidget*>{button, input}) {
        widget->SetSize(100, 30);
        widget->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft,
                         widget == button ? 10.0f : 120.0f, 10);
        widget->SetFocusRingColor(0xFFCC00FFu);
    }
    const auto ringCount = [&] {
        manager.Render();
        return std::count_if(renderer->commands.begin(), renderer->commands.end(),
            [](const UIRenderCommand& command) { return command.color == 0xFFCC00FFu; });
    };
    manager.Update(0);
    EXPECT_EQ(ringCount(), 0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(ringCount(), 4);
    const auto lastRing = std::find_if(renderer->commands.rbegin(), renderer->commands.rend(),
        [](const UIRenderCommand& command) { return command.color == 0xFFCC00FFu; });
    ASSERT_NE(lastRing, renderer->commands.rend());
    EXPECT_FLOAT_EQ(lastRing->x, 108);
    button->SetFocusRingEnabled(false);
    EXPECT_EQ(ringCount(), 0);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    button->SetFocusRingEnabled(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(ringCount(), 4);
    EXPECT_FLOAT_EQ(renderer->commands.back().x, 218);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    EXPECT_EQ(ringCount(), 4);
    button->SetKeyboardEnabled(false);
    EXPECT_EQ(ringCount(), 0);
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
    manager.InjectMouseMove(125, 15);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    EXPECT_EQ(ringCount(), 4);
    input->SetVisible(false);
    EXPECT_EQ(ringCount(), 0);
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
}

TEST(FocusVisualization, RingRespectsChildPaintingSiblingOrderAndAncestorClip) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100, 40);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetContentSize(100, 100);
    scroll->SetScrollbarsEnabled(false);
    auto* button = scroll->GetContent()->CreateChild<UIButton>("Button");
    button->SetSize(80, 30);
    button->SetPoint(AnchorPoint::TopLeft, scroll->GetContent(), AnchorPoint::TopLeft, 0, 20);
    button->SetFocusRingColor(0xFFCC00FFu);
    auto* child = button->CreateChild<UITextureWidget>("Child");
    child->SetAllPoints(button);
    child->SetTint(0x123456FFu);
    auto* later = scroll->GetContent()->CreateChild<UITextureWidget>("LaterSibling");
    later->SetAllPoints(button);
    later->SetTint(0x654321FFu);
    manager.Update(0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    scroll->SetScrollOffset(0, 0);
    manager.Update(0);
    manager.Render();
    int clipDepth = 0;
    int ringCount = 0;
    bool childPainted = false;
    bool siblingPainted = false;
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::PushScissor) ++clipDepth;
        if (command.type == RenderCommandType::PopScissor) --clipDepth;
        if (command.color == 0x123456FFu) childPainted = true;
        if (command.color == 0x654321FFu) siblingPainted = true;
        if (command.color != 0xFFCC00FFu) continue;
        ++ringCount;
        EXPECT_GT(clipDepth, 0);
        EXPECT_TRUE(childPainted);
        EXPECT_FALSE(siblingPainted);
    }
    EXPECT_EQ(ringCount, 4);
    EXPECT_EQ(clipDepth, 0);
    EXPECT_TRUE(siblingPainted);
}

TEST(FocusVisualization, TreePopupRestorationAndTinyBoundsUseSharedDecoration) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("Tree");
    tree->SetSize(100, 60);
    tree->SetPadding({5, 5, 5, 5});
    tree->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tree->SetFocusRingColor(0xFFCC00FFu);
    tree->AddNode(0, "Item");
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Menu");
    menu->SetItems({{"Item", {}}});
    manager.Update(0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Render();
    EXPECT_EQ(manager.GetFocusedWidget(), tree);
    EXPECT_EQ(renderer->commands.back().color, 0xFFCC00FFu);
    int clipDepth = 0;
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::PushScissor) ++clipDepth;
        if (command.type == RenderCommandType::PopScissor) --clipDepth;
        if (command.color == 0xFFCC00FFu) EXPECT_EQ(clipDepth, 0);
    }
    menu->Open(110, 10, tree);
    manager.GetFocusedWidget()->SetFocusRingColor(0xCC00FFFFu);
    manager.Render();
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0xFFCC00FFu; }), 0);
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0xCC00FFFFu; }), 4);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    manager.Render();
    EXPECT_EQ(manager.GetFocusedWidget(), tree);
    EXPECT_EQ(renderer->commands.back().color, 0xFFCC00FFu);
    tree->SetSize(0.5f, 0.5f);
    tree->SetPadding({});
    manager.Update(0);
    manager.Render();
    int tinyStrokes = 0;
    for (const auto& command : renderer->commands) {
        if (command.color != 0x172127FFu) continue;
        ++tinyStrokes;
        EXPECT_GT(command.width, 0);
        EXPECT_GT(command.height, 0);
        EXPECT_GE(command.x, 0);
        EXPECT_GE(command.y, 0);
        EXPECT_LE(command.x + command.width, 0.5f);
        EXPECT_LE(command.y + command.height, 0.5f);
    }
    EXPECT_EQ(tinyStrokes, 2);
    tree->SetSize(0, 0);
    manager.Update(0);
    manager.Render();
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0x172127FFu; }), 0);
}

TEST(FocusVisualization, ButtonShowsPairedKeyboardPressAndCancelsOnFocusLoss) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetSize(100.0f, 30.0f);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    button->SetNormalColor(0x202020FFu);
    button->SetPressedColor(0x909090FFu);
    manager.GetRoot().CreateChild<UIButton>("Next");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.Render();
    ASSERT_FALSE(renderer->commands.empty());
    EXPECT_EQ(renderer->commands.front().color, 0x909090FFu);
    EXPECT_EQ(button->GetState(), ControlState::Normal);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_TRUE(button->IsKeyboardPressed());
    manager.InjectKeyEvent(ScanCode::Enter, false);
    manager.Render();
    EXPECT_EQ(renderer->commands.front().color, 0x202020FFu);
    EXPECT_EQ(clicks, 1);
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    manager.Render();
    EXPECT_FALSE(button->IsKeyboardPressed());
    EXPECT_EQ(renderer->commands.front().color, 0x202020FFu);
    EXPECT_EQ(clicks, 1);
}

TEST(KeyboardNavigation, TabFocusesInputsInTreeOrderAndWraps) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIInputBox>("First");
    auto* second = manager.GetRoot().CreateChild<UIInputBox>("Second");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'a');
    manager.InjectKeyEvent(ScanCode::Tab, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'b');
    manager.InjectKeyEvent(ScanCode::Tab, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'c');
    EXPECT_EQ(first->GetText(), "ac");
    EXPECT_EQ(second->GetText(), "b");
}

TEST(KeyboardNavigation, ShiftTabSkipsHiddenAndDisabledControls) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIButton>("First");
    auto* disabled = manager.GetRoot().CreateChild<UICheckBox>("Disabled");
    disabled->SetEnabled(false);
    auto* hidden = manager.GetRoot().CreateChild<UIWidget>("Hidden");
    hidden->CreateChild<UIInputBox>("HiddenInput");
    hidden->SetVisible(false);
    auto* last = manager.GetRoot().CreateChild<UIInputBox>("Last");
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::RightShift, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    manager.InjectKeyEvent(ScanCode::Tab, false);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), first);
    manager.InjectKeyEvent(ScanCode::RightShift, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    last->SetVisible(false);
    manager.InjectCharacter(U'x');
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
    EXPECT_FALSE(last->IsFocused());
    EXPECT_TRUE(last->GetText().empty());
}

TEST(KeyboardNavigation, ActivationRequiresPairedKeysAndCancelsOnFocusLoss) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    auto* checkbox = manager.GetRoot().CreateChild<UICheckBox>("Checkbox");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_FALSE(checkbox->IsChecked());
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_TRUE(checkbox->IsChecked());
    EXPECT_EQ(clicks, 1);
}

TEST(KeyboardNavigation, ArrowsAdjustSliderAndEditUtf8AtCursor) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* slider = manager.GetRoot().CreateChild<UISlider>("Slider");
    slider->SetMinMaxValues(0.0f, 100.0f);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_FLOAT_EQ(slider->GetValue(), 1.0f);
    manager.InjectKeyEvent(ScanCode::End, true);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_FLOAT_EQ(slider->GetValue(), 100.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'a');
    manager.InjectCharacter(U'\u00E9');
    manager.InjectCharacter(U'\U0001F600');
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(input->GetText(), "a\xF0\x9F\x98\x80");
    manager.InjectKeyEvent(ScanCode::Delete, true);
    EXPECT_EQ(input->GetText(), "a");
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectCharacter(U'b');
    EXPECT_EQ(input->GetText(), "ba");
    EXPECT_EQ(manager.GetFocusedWidget(), input);
}

TEST(KeyboardNavigation, DropdownAndTabsKeepCompositeFocus) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>("Dropdown");
    dropdown->SetOptions({"First", "Second", "Third"});
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>("Tabs");
    auto* firstInput = tabs->AddTab("First")->CreateChild<UIInputBox>("FirstInput");
    auto* secondInput = tabs->AddTab("Second")->CreateChild<UIInputBox>("SecondInput");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(dropdown->GetSelectedIndex(), 1);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_TRUE(dropdown->IsExpanded());
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_FALSE(dropdown->IsExpanded());
    EXPECT_EQ(manager.GetFocusedWidget(), tabs);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(tabs->GetSelectedIndex(), 1);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), secondInput);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(tabs->GetSelectedIndex(), 1);
    EXPECT_FALSE(firstInput->IsFocused());
}

TEST(KeyboardNavigation, TreeArrowsSelectExpandCollapseAndRevealRows) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("Tree");
    tree->SetSize(200.0f, 48.0f);
    tree->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    const auto parent = tree->AddNode(0, "Parent");
    const auto child = tree->AddNode(parent, "Child");
    const auto last = tree->AddNode(0, "Last");
    manager.Update(0.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(tree->GetSelectedNode(), parent);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(tree->GetSelectedNode(), child);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(tree->GetSelectedNode(), parent);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_FALSE(tree->IsExpanded(parent));
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_TRUE(tree->IsExpanded(parent));
    manager.InjectKeyEvent(ScanCode::End, true);
    EXPECT_EQ(tree->GetSelectedNode(), last);
    EXPECT_GT(tree->GetScrollY(), 0.0f);
}

TEST(KeyboardNavigation, TabRevealsOffscreenControls) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 50.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetContentSize(100.0f, 300.0f);
    auto* button = scroll->GetContent()->CreateChild<UIButton>("Offscreen");
    button->SetSize(60.0f, 20.0f);
    button->SetPoint(AnchorPoint::TopLeft, scroll->GetContent(), AnchorPoint::TopLeft, 0.0f, 200.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 170.0f);
    EXPECT_FLOAT_EQ(button->GetComputedRect().y, 30.0f);
}

TEST(KeyboardNavigation, PopupArrowsSkipUnavailableItemsAndRestoreFocus) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(200.0f, 60.0f);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Menu");
    int first = 0;
    int last = 0;
    menu->SetItems({{"First", [&] { ++first; }}, {"Disabled", {}, false}, {"", {}, true, true}, {"Last", [&] { ++last; }}});
    manager.InjectKeyEvent(ScanCode::Tab, true);
    menu->Open(0.0f, 0.0f);
    EXPECT_FALSE(input->IsFocused());
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_GT(menu->GetScrollY(), 0.0f);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(last, 1);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    menu->Open(0.0f, 0.0f);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectKeyEvent(ScanCode::Up, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(first, 1);
    menu->Open(0.0f, 0.0f);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    EXPECT_FALSE(menu->IsOpen());
}

TEST(KeyboardNavigation, MenuBarArrowsSwitchOpenMenus) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* bar = manager.GetRoot().CreateChild<UIMenuBar>(manager, "Bar");
    auto* first = bar->AddMenu("First", {{"Item", {}}});
    int clicks = 0;
    auto* second = bar->AddMenu("Second", {{"Item", [&] { ++clicks; }}});
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), bar);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_TRUE(first->IsOpen());
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_FALSE(first->IsOpen());
    EXPECT_TRUE(second->IsOpen());
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(manager.GetFocusedWidget(), bar);
}

TEST(KeyboardNavigation, RadioArrowsMoveFocusAndSelectionWithinEnabledGroup) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIRadioButton>("First");
    auto* disabled = manager.GetRoot().CreateChild<UIRadioButton>("Disabled");
    disabled->SetEnabled(false);
    auto* other = manager.GetRoot().CreateChild<UIRadioButton>("Other");
    other->SetGroup("Other");
    auto* last = manager.GetRoot().CreateChild<UIRadioButton>("Last");
    first->SetChecked(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    EXPECT_TRUE(last->IsChecked());
    EXPECT_FALSE(first->IsChecked());
    EXPECT_FALSE(other->IsChecked());
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(manager.GetFocusedWidget(), first);
    EXPECT_TRUE(first->IsChecked());
    EXPECT_FALSE(last->IsChecked());
}

TEST(EventRouting, ClickOnWidgetFiresOnClick) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIButton* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetSize(100.0f, 40.0f);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    bool clicked = false;
    button->RegisterCallback(UIEventType::OnClick, [&clicked](const UIEventData&) { clicked = true; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_TRUE(clicked);
}

TEST(EventRouting, ReleasingOutsideOriginalWidgetDoesNotFireClick) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIButton* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetSize(100.0f, 40.0f);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    bool clicked = false;
    button->RegisterCallback(UIEventType::OnClick, [&clicked](const UIEventData&) { clicked = true; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(500.0f, 500.0f);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_FALSE(clicked);
}

TEST(EventRouting, TopMostOverlappingWidgetCapturesHitFirst) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIButton* back = manager.GetRoot().CreateChild<UIButton>("Back");
    back->SetSize(100.0f, 100.0f);
    back->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    UIButton* front = manager.GetRoot().CreateChild<UIButton>("Front");
    front->SetSize(50.0f, 50.0f);
    front->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    bool backClicked = false;
    bool frontClicked = false;
    back->RegisterCallback(UIEventType::OnClick, [&backClicked](const UIEventData&) { backClicked = true; });
    front->RegisterCallback(UIEventType::OnClick, [&frontClicked](const UIEventData&) { frontClicked = true; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_TRUE(frontClicked);
    EXPECT_FALSE(backClicked);
}

TEST(EventRouting, InjectedClickBubblesFromLeafToRootWithOriginalPayload) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    parent->SetSize(100.0f, 100.0f);
    parent->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    UIWidget* leaf = parent->CreateChild<UIWidget>("Leaf");
    leaf->SetAllPoints(parent);

    std::vector<std::string> visited;
    for (UIWidget* widget : {leaf, parent, &manager.GetRoot()}) {
        widget->RegisterCallback(UIEventType::OnClick, [&, widget](const UIEventData& data) {
            visited.push_back(widget->GetName());
            EXPECT_EQ(data.type, UIEventType::OnClick);
            EXPECT_FLOAT_EQ(data.mouseX, 10.0f);
            EXPECT_FLOAT_EQ(data.mouseY, 20.0f);
            EXPECT_EQ(data.button, MouseButton::Right);
            EXPECT_FALSE(data.isDown);
        });
    }

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 20.0f);
    manager.InjectMouseButton(MouseButton::Right, true);
    manager.InjectMouseButton(MouseButton::Right, false);

    EXPECT_EQ(visited, (std::vector<std::string>{"Leaf", "Parent", "Root"}));
}

TEST(EventRouting, HandledEventStopsAtParent) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    UIWidget* leaf = parent->CreateChild<UIWidget>("Leaf");
    std::vector<std::string> visited;
    parent->RegisterCallback(UIEventType::OnClick, [&](const UIEventData& data) {
        visited.push_back("Parent");
        data.handled = true;
    });
    manager.GetRoot().RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) {
        visited.push_back("Root");
    });

    UIEventData data{UIEventType::OnClick};
    leaf->FireEvent(data);

    EXPECT_TRUE(data.handled);
    EXPECT_EQ(visited, (std::vector<std::string>{"Parent"}));
}

TEST(EventRouting, MouseEnterAndLeaveRemainTargetLocal) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* leaf = manager.GetRoot().CreateChild<UIWidget>("Leaf");
    int leafEvents = 0;
    int parentEvents = 0;
    for (UIEventType type : {UIEventType::OnMouseEnter, UIEventType::OnMouseLeave}) {
        leaf->RegisterCallback(type, [&](const UIEventData&) { ++leafEvents; });
        manager.GetRoot().RegisterCallback(type, [&](const UIEventData&) { ++parentEvents; });
        leaf->FireEvent(UIEventData{type});
    }

    EXPECT_EQ(leafEvents, 2);
    EXPECT_EQ(parentEvents, 0);
}

TEST(EventRouting, HandledLeafClickDoesNotReachAncestors) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* leaf = manager.GetRoot().CreateChild<UIWidget>("Leaf");
    int parentClicks = 0;
    leaf->RegisterCallback(UIEventType::OnClick, [](const UIEventData& data) {
        data.handled = true;
    });
    manager.GetRoot().RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) {
        ++parentClicks;
    });

    leaf->FireEvent(UIEventData{UIEventType::OnClick});

    EXPECT_EQ(parentClicks, 0);
}

TEST(EventRouting, CapturedReleaseBubblesButReleaseOutsideDoesNotClick) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    parent->SetSize(100.0f, 100.0f);
    parent->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    UIWidget* leaf = parent->CreateChild<UIWidget>("Leaf");
    leaf->SetAllPoints(parent);
    std::vector<UIEventType> events;
    for (UIEventType type : {UIEventType::OnMouseDown, UIEventType::OnMouseUp, UIEventType::OnClick}) {
        parent->RegisterCallback(type, [&](const UIEventData& data) {
            events.push_back(data.type);
            EXPECT_EQ(data.isDown, data.type == UIEventType::OnMouseDown);
            if (data.type == UIEventType::OnMouseUp) {
                EXPECT_FLOAT_EQ(data.mouseX, 500.0f);
            }
        });
    }

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(500.0f, 500.0f);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_EQ(events, (std::vector<UIEventType>{UIEventType::OnMouseDown, UIEventType::OnMouseUp}));
}

TEST(EventRouting, ControlConsumesPressAndReleaseButStillRunsItsCallbacks) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIButton* parent = manager.GetRoot().CreateChild<UIButton>("Parent");
    UIButton* button = parent->CreateChild<UIButton>("Button");
    UIWidget* label = button->CreateChild<UIWidget>("Label");
    int callbacks = 0;
    for (UIEventType type : {UIEventType::OnMouseDown, UIEventType::OnMouseUp}) {
        button->RegisterCallback(type, [&](const UIEventData& data) {
            EXPECT_TRUE(data.handled);
            ++callbacks;
        });
        label->FireEvent(UIEventData{type});
        EXPECT_EQ(parent->GetState(), ControlState::Normal);
    }

    EXPECT_EQ(button->GetState(), ControlState::Hovered);
    EXPECT_EQ(callbacks, 2);
}

TEST(EventRouting, DropdownOptionSelectionConsumesClickWithoutReopening) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIDropDownBox* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>("Dropdown");
    dropdown->SetSize(100.0f, 30.0f);
    dropdown->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    dropdown->SetOptions({"First", "Second"});
    int dropdownClicks = 0;
    int rootClicks = 0;
    int valueChanges = 0;
    dropdown->RegisterCallback(UIEventType::OnClick, [&](const UIEventData& data) {
        EXPECT_TRUE(data.handled);
        ++dropdownClicks;
    });
    manager.GetRoot().RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++rootClicks; });
    manager.GetRoot().RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++valueChanges; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    ASSERT_TRUE(dropdown->IsExpanded());
    manager.InjectMouseMove(10.0f, 60.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_EQ(dropdown->GetSelectedIndex(), 1);
    EXPECT_FALSE(dropdown->IsExpanded());
    EXPECT_EQ(dropdownClicks, 1);
    EXPECT_EQ(rootClicks, 0);
    EXPECT_EQ(valueChanges, 1);
}

TEST(EventRouting, WidgetGeneratedEventsBubbleIndependently) {
    UIManager manager(std::make_shared<NullRenderer>());
    UISlider* slider = manager.GetRoot().CreateChild<UISlider>("Slider");
    int valueChanges = 0;
    manager.GetRoot().RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData& data) {
        ++valueChanges;
        data.handled = true;
    });

    slider->SetValue(0.25f);
    slider->SetValue(0.75f);

    EXPECT_EQ(valueChanges, 2);
}
